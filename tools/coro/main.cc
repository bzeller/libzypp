#include "zypp-core/zyppng/base/eventdispatcher.h"
#include <zypp-core/zyppng/base/eventloop.h>
#include <zypp-core/zyppng/base/timer.h>
#include <zypp-core/zyppng/pipelines/asyncresult.h>
#include <zypp-core/zyppng/pipelines/expected.h>
#include <zypp-core/fs/TmpPath.h>
#include <zypp-core/Url.h>
#include <zypp-core/Pathname.h>
#include <zypp-media/ng/provide.h>

#include <coroutine>
#include <iostream>

template <typename T> struct awaiter;

// Enable the use of AsyncOpRef<T> as a coroutine type
template<typename T, typename... Args>
requires(!std::is_void_v<T> && !std::is_reference_v<T>)
struct std::coroutine_traits<zyppng::AsyncOpRef<T>, Args...>
{
  struct promise_type;

  struct CoroAsyncOp : public zyppng::AsyncOp<T>
  {
    CoroAsyncOp( std::coroutine_handle<promise_type> coro ) : _coroHandle( std::move(coro) ) {
      // BUG ! this would not work if the CoroAsyncOp is used in a pipeline!
      this->sigReady().connect([this](){ _markedDone = true; });
    }
    ~CoroAsyncOp() override {
      // if the coroutine is not done, make sure to destroy it when the promise handle is destroyed
      std::cout << "Coroutine handle valid: " << _coroHandle.operator bool() << " marked done: " << _markedDone << std::endl;
      if ( _markedDone )
        _coroHandle.destroy();
    }
    bool _markedDone = false;
    std::coroutine_handle<promise_type> _coroHandle;
  };

  struct promise_type
  {
    zyppng::AsyncOpRef<T> get_return_object() noexcept
    {
      auto res = _ret_obj.lock();
      if ( !res ) {
        std::cout << "Making new ret object" << std::endl;
        res = std::make_shared<CoroAsyncOp>( std::coroutine_handle<promise_type>::from_promise(*this) );
      } else {
        std::cout << "Returning existing ret object " << std::endl;
      }
      _ret_obj = res;
      return res;
    }

    // we should be able to return awaiter<T> here but that crashes the compiler
    std::suspend_never initial_suspend() const noexcept { return {}; }
    std::suspend_never final_suspend() const noexcept { return {}; }

    void return_value(const T& value)
        noexcept(std::is_nothrow_copy_constructible_v<T>)
    {
      _ret_obj.lock()->setReady( T(value) );
    }

    void return_value(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>)
    {
      _ret_obj.lock()->setReady( std::move(value) );
    }

    void unhandled_exception() noexcept
    {
      if constexpr( zyppng::is_instance_of_v<zyppng::expected, T> ) {
        _ret_obj.lock()->setReady( T::error( std::current_exception() ) );
      } else {
        std::terminate();
      }
    }

  private:
    std::weak_ptr<zyppng::AsyncOp<T>> _ret_obj;
  };
};

template <typename T>
struct awaiter
{
  awaiter( zyppng::AsyncOpRef<T> fut ) : _fut( std::move(fut) ) {}
  ~awaiter() {
    if ( !_fut->isReady() ) {
      std::cout << "Non ready stuff" << std::endl;
    } else {
      std::cout << "Awaiter destroyed" << std::endl;
    }

  }
  bool await_ready() const noexcept
  {
    return _fut->isReady();
  }

  void await_suspend(std::coroutine_handle<> cont) const
  {
    std::cout << "Suspending with use count: " << _fut.use_count() << std::endl;
    _fut->sigReady().connect( [c = std::move(cont)] () mutable {
      zyppng::EventDispatcher::invokeOnIdle( [c = std::move(c)](){ c.resume(); return false;} );
    } );
  }

  T await_resume() {
    std::cout << "Resuming with use count: " << _fut.use_count() << std::endl;
    return _fut->get();
  }

  zyppng::AsyncOpRef<T> _fut;
};

// Allow co_await'ing AsyncOpRef<T>
template<typename T>
auto operator co_await( zyppng::AsyncOpRef<T> future ) noexcept
requires(!std::is_reference_v<T>)
{
  return awaiter { std::move(future) };
}

zyppng::AsyncOpRef<zyppng::expected<zypp::ManagedFile>> downloadFile ( zyppng::ProvideRef p, zypp::Url mediaUrl, zypp::Pathname path )
{
  using namespace zyppng::operators;

  auto media = co_await p->attachMedia( mediaUrl, zyppng::ProvideMediaSpec() );
  if ( !media )
    co_return zyppng::expected<zypp::ManagedFile>::error( media.error() );

  std::cout << "Coro Attached" << std::endl;

  auto provRes = co_await p->provide( media.get(), path, zyppng::ProvideFileSpec() );
  if ( !provRes )
    co_return zyppng::expected<zypp::ManagedFile>::error( provRes.error() );

  std::cout << "Coro Provided" << std::endl;

  auto file = co_await p->copyFile( std::move(provRes.get()), zypp::Pathname("/tmp") / path.basename() );

  std::cout << "Coro copied: " << file.is_valid() << std::endl;
#if 0
  auto file = co_await(
    p->attachMedia( mediaUrl, zyppng::ProvideMediaSpec() )
    | and_then([&]( zyppng::ProvideMediaHandle h ){
      return p->provide( h, path, zyppng::ProvideFileSpec() );
    })
    | and_then( zyppng::Provide::copyResultToDest(p, zypp::Pathname("/tmp") / path.basename() ) )
  );
#endif
  co_return file;
}

int main ( int argc, char * argv[] ) {
  auto evLoop = zyppng::EventLoop::create();

  zypp::filesystem::TmpDir tmp;
  auto prov   = zyppng::Provide::create( tmp.path() );
  prov->start();

  auto op = downloadFile( prov, zypp::Url("https://download.opensuse.org/tumbleweed/repo/debug"), "/media.1/media" );
  op->sigReady().connect([&](){
    evLoop->quit();
  });

  std::cout << "Before EV " << op->isReady() << std::endl;

  if ( !op->isReady() )
    evLoop->run();

  std::cout << "After EV " << op->isReady() << std::endl;
  std::cout << "Result valid:"   << op->get().is_valid() << std::endl;
  op->get()->resetDispose();
  return 0;
}
