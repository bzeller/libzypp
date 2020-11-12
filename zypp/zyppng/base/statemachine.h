/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\----------------------------------------------------------------------/
*
* This file contains private API, this might break at any time between releases.
* You have been warned!
*
*/
#ifndef ZYPP_NG_BASE_STATEMACHINE_INCLUDED_H
#define ZYPP_NG_BASE_STATEMACHINE_INCLUDED_H

#include <zypp/zyppng/base/signals.h>

#include <variant>
#include <tuple>
#include <functional>
#include <memory>
#include <iostream>

namespace zyppng {


  /**

    enum Sid {};

    class StateA;
    class StateB;
    class StateC;

    auto sm = Statemachine< Sid,
      Transition<StateA, &StateA::eventA, StateB, []{return true;}, [](){ / * do stuff * /} >,
      Transition<StateB, &StateB::eventB, StateC, []{return true;}, [](){ / * do stuff * /} >
      Transition<StateB, &StateA::eventC, StateC, []{return true;}, [](){ / * do stuff * /} >
    >();

   */

  namespace detail {

    template <typename T>
    using EventSource = signal<void()> & (T::*)();

    /**
     * \internal
     * Internal helper type to wrap the user implemented state types.
     * It's mostly used to have a container for the Transitions that belong to the State.
     */
    template < class State, class Transitions >
    struct StateWithTransitions {
      using StateType = State;
      Transitions _transitions;

      template< typename StateMachine >
      StateWithTransitions ( StateMachine &sm ) : _ptr ( std::make_unique<State>( sm )) { }
      StateWithTransitions ( std::unique_ptr<State> &&s ) : _ptr ( std::move(s) ) {}

      // move construction is ok
      StateWithTransitions ( StateWithTransitions &&other ) = default;
      StateWithTransitions &operator= ( StateWithTransitions &&other ) = default;

      // no copy construction
      StateWithTransitions ( const StateWithTransitions &other ) = delete;
      StateWithTransitions &operator= ( const StateWithTransitions &other ) = delete;

      static constexpr auto stateId = State::stateId;
      static constexpr bool isFinal = State::isFinal;

      void enter( ) {
        return _ptr->enter( );
      }

      void exit( ) {
        return _ptr->exit( );
      }

      operator State& () {
        return wrappedState();
      }

      State &wrappedState () {
        return *_ptr;
      }

    private:
      std::unique_ptr<State> _ptr;
    };


    /**
     * this adds the type \a NewType to the collection if the condition is true
     */
    template < template<typename...> typename Templ , typename NewType, typename TupleType, bool condition >
    struct add_type_to_collection;

    template < template<typename...> typename Templ, typename NewType, typename ...Types >
    struct add_type_to_collection< Templ, NewType, Templ<Types...>, true > {
      using Type = Templ<Types..., NewType>;
    };

    template < template<typename...> typename Templ, typename NewType, typename ...Types >
    struct add_type_to_collection< Templ, NewType, Templ<Types...>, false > {
      using Type = Templ<Types...>;
    };

    /**
     * Constexpr function that evaluates to true if a variant type \a Variant already contains the type \a Type.
     * The \a Compare argument can be used to change how equality of a type is calculated
     */
    template < typename Variant, typename Type, template<typename, typename> typename Compare = std::is_same, size_t I = 0 >
    constexpr bool VariantHasType () {
      // cancel the evaluation if we entered the last type in the variant
      if constexpr ( I >= std::variant_size_v<Variant> ) {
        return false;
      } else {
        // if the current type in the variant is the same as the one we are looking for evaluate to true
        if ( Compare< std::variant_alternative_t< I, Variant>, Type >::value )
          return true;

        // otherwise call the next iteration with I+1
        return VariantHasType<Variant, Type, Compare, I+1>();
      }
    }

    /**
     * collect all transitions that have the same SourceState as the first type argument
     */
    template< class State, class TupleSoFar, class Head, class ...Transitions >
    struct collect_transitions_helper {
      using NewTuple = typename add_type_to_collection< std::tuple, Head, TupleSoFar, std::is_same_v< State, typename Head::SourceType> >::Type;
      using Type = typename collect_transitions_helper<State, NewTuple, Transitions...>::Type;
    };

    template< class State, class TupleSoFar, class Head >
    struct collect_transitions_helper<State, TupleSoFar, Head > {
      using Type = typename add_type_to_collection< std::tuple, Head, TupleSoFar, std::is_same_v< State, typename Head::SourceType> >::Type;
    };

    template< class State, class ...Transitions >
    struct collect_transitions{
      using Type = typename collect_transitions_helper< State, std::tuple<>, Transitions... >::Type;
    };

    /**
     * Iterates over the list of Transitions and collects them all in a std::variant<State1, State2, ...> type
     */
    template <typename VariantSoFar, typename Head, typename ...Transitions>
    struct make_state_set_helper {
      using WithSource = typename add_type_to_collection< std::variant, typename Head::SourceType, VariantSoFar, !VariantHasType<VariantSoFar, typename Head::SourceType>() >::Type;
      using WithTarget = typename add_type_to_collection< std::variant, typename Head::TargetType, WithSource, !VariantHasType<WithSource, typename Head::TargetType>() >::Type;
      using Type = typename make_state_set_helper<WithTarget, Transitions...>::Type;
    };

    template <typename VariantSoFar, typename Head>
    struct make_state_set_helper< VariantSoFar, Head > {
      using WithSource = typename add_type_to_collection< std::variant, typename Head::SourceType, VariantSoFar, !VariantHasType<VariantSoFar, typename Head::SourceType>() >::Type;
      using Type = typename add_type_to_collection< std::variant, typename Head::TargetType, WithSource, !VariantHasType<WithSource, typename Head::TargetType>() >::Type;
    };

    template <typename Head, typename ...Transitions>
    struct make_state_set {
      using InitialVariant = std::variant<typename Head::SourceType>;
      using VariantSoFar = typename add_type_to_collection< std::variant, typename Head::TargetType, InitialVariant , !VariantHasType<InitialVariant, typename Head::TargetType>() >::Type;
      using Type = typename make_state_set_helper< VariantSoFar, Transitions...>::Type;
    };


    /**
     * Evaluates to true if type \a A and type \a B wrap the same State type
     */
    template <typename A, typename B>
    struct is_same_state : public std::is_same< typename A::StateType, typename B::StateType> {};


    /**
     * Turns a State type into its StateWithTransitions counterpart
     */
    template <typename State, typename ...Transitions>
    struct make_statewithtransition {
      using Type = StateWithTransitions<State, typename collect_transitions<State, Transitions...>::Type>;
    };

    /**
     * Iterates over each State in the \a StateVariant argument, collects the corresponding
     * Transitions and combines the results in a std::variant< StateWithTransitions<...>,... > type.
     */
    template <typename VariantSoFar, typename StateVariant, typename ...Transitions>
    struct make_statewithtransition_set_helper;

    template <typename VariantSoFar, typename HeadState, typename ...State, typename ...Transitions>
    struct make_statewithtransition_set_helper< VariantSoFar, std::variant<HeadState, State...>, Transitions... > {
      using FullStateType = typename make_statewithtransition<HeadState, Transitions...>::Type;
      using NewVariant = typename add_type_to_collection< std::variant, FullStateType, VariantSoFar, !VariantHasType<VariantSoFar, FullStateType/*, is_same_state */>()>::Type;
      using Type = typename make_statewithtransition_set_helper< NewVariant, std::variant<State...>, Transitions...>::Type;
    };

    template <typename VariantSoFar, typename HeadState, typename ...Transitions >
    struct make_statewithtransition_set_helper< VariantSoFar, std::variant<HeadState>, Transitions... > {
      using FullStateType = typename make_statewithtransition<HeadState, Transitions...>::Type;
      using Type = typename add_type_to_collection< std::variant, FullStateType, VariantSoFar, !VariantHasType<VariantSoFar, FullStateType /*, is_same_state */>()>::Type;
    };

    template <typename NoState, typename StateVariant, typename ...Transitions>
    struct make_statewithtransition_set;

    template <typename NoState, typename HeadState, typename ...States, typename ...Transitions>
    struct make_statewithtransition_set< NoState, std::variant<HeadState, States...>, Transitions...>{
      using FirstState = typename make_statewithtransition< HeadState, Transitions...>::Type;
      using Type = typename make_statewithtransition_set_helper< std::variant<NoState, FirstState>, std::variant<States...>, Transitions...>::Type;
    };
  }


  /**
   * The default transition condition if none was explictely defined.
   * Returns always true.
   */
  template < typename Source >
  bool defaultCondition ( Source & ) {
    return true;
  }

  /**
   * The default transition operation if none was explictely defined.
   * Returns a new instance of the \a Target state type.
   */
  template <typename Source, typename Target >
  struct defaultOperation {

    constexpr defaultOperation(){};

    template< typename Statemachine >
    std::unique_ptr<Target> operator() ( Statemachine &sm, Source & ) {
      return std::make_unique<Target>( sm );
    }

  };

  template <typename Source, typename Target >
  constexpr static auto defaultOp = defaultOperation<Source,Target>();

  /*!
   * Defines a transition between \a Source and \a Target states.
   * The EventSource \a ev triggers the transition from Source to Target if the condition \a Cond
   * evaluates to true. The operation \a Op is called between exiting the old and entering the new state.
   * It can be used to transfer informations from the old into the new state.
   *
   * \tparam Source defines the type of the Source state
   * \tparam ev takes a member function pointer returning the event trigger signal that is used to trigger the transition to \a Target
   * \tparam Target defines the type of the Target state
   * \tparam Cond Defines the transition condition, can be used if the same event could trigger different transitions
   *         based on a condition
   * \tparam Op defines the transition operation from Source to Target states,
   *         this is either a function with the signature:   std::unique_ptr<Target> ( Statemachine &, Source & )
   *         or it can be a member function pointer of Source with the signature:  std::unique_ptr<Target> ( Source::* ) ( Statemachine & )
   */
  template <
    typename Source,
    detail::EventSource<Source> ev ,
    typename Target,
    auto Cond = defaultCondition<Source>,
    auto Op   = &defaultOp<Source, Target> >
  struct Transition {

    using SourceType = Source;
    using TargetType = Target;


    template< typename Statemachine >
    std::unique_ptr<Target> operator() ( Statemachine &sm, Source &oldState ) {
      using OpType = decltype ( Op );
      // check if we have a member function pointer
      if  constexpr ( std::is_invocable_v< OpType, Source *, Statemachine & > ) {
        return std::invoke( Op, &oldState, sm );
      } else {
        return std::invoke( Op, sm, oldState );
      }
    }

    bool checkCondition ( Source &currentState ) {
      return std::invoke( Cond, currentState );
    }

    signal< void() >& eventSource ( Source *st ) {
      return std::invoke( ev, st );
    }

  };



  /*!
   * \brief This defines the actual StateMachine.
   * \tparam Derived is the Statemachine subclass type, this is used to pass a reference to the actual implementation into the State functions.
   * \tparam StateId should be a enum with a ID for each state the SM can be in.
   * \tparam Transitions variadic template argument taking a list of all \ref Transition types the statemachine should support
   *
   */
  template < typename Derived, typename StateId, typename ...Transitions >
  class Statemachine {

    struct InitialState{};

  public:

    using AllStates = typename detail::make_state_set< Transitions... >::Type;
    using StateSetHelper = typename detail::make_statewithtransition_set< InitialState, AllStates, Transitions... >;
    using FState = typename StateSetHelper::FirstState;
    using StateSet = typename StateSetHelper::Type;

    public:
      Statemachine () { }

      void start () {
        if ( _state.index() == 0 )
          enterState( FState( static_cast<Derived &>(*this) ) );
      }

      StateId currentState () const {
        return std::visit( [this]( const auto &s ) -> StateId {
          using T = std::decay_t<decltype (s)>;
          if constexpr ( std::is_same_v< T, InitialState > ) {
            throw std::exception();
          } else {
            return T::stateId;
          }
        }, _state );
      }

      template<typename T>
      T& state () {
        if ( _state.index() == 0 ) {
          throw std::exception();
        } else {
          using WrappedEventType = typename detail::make_statewithtransition< T, Transitions...>::Type;
          return std::get<WrappedEventType>( _state );
        }
      }

      SignalProxy<void()> sigFinished () {
        return _sigFinished;
      }

      SignalProxy<void ( StateId )> sigStateChanged () {
        return _sigStateChanged;
      }

    protected:

      template <typename OldState, typename NewState>
      void enterState ( OldState &os, NewState &&nS ) {
        std::forward<OldState>(os).exit();
        enterState( std::forward<NewState>(nS) );
      }

      template <typename NewState>
      void enterState ( NewState &&nS ) {

        connectAllTransitions<0>( nS, nS._transitions );

        bool emitFin = nS.isFinal;
        _state = std::forward<NewState>(nS);

        std::get< std::decay_t<NewState> >( _state ).enter();

        // let the outside world know whats going on
        _sigStateChanged.emit( NewState::stateId  );
        if ( emitFin ) _sigFinished.emit();
      }

      template <typename State, typename Transition>
      auto makeEventCallback ( Transition &transition ) {
        using WrappedEventType = typename detail::make_statewithtransition< typename Transition::TargetType, Transitions...>::Type;
        return [ mytrans = &transition, this]() mutable {
          if ( mytrans->checkCondition( std::get< State >(_state) ) ) {
            auto &st = std::get< std::decay_t<State> >(_state);
            enterState( st , WrappedEventType( (*mytrans)( static_cast<Derived &>(*this), st ) ) );
          }
        };
      }

      template< std::size_t I = 0, typename State, typename ...StateTrans>
      void connectAllTransitions( State &&nS, std::tuple<StateTrans...> &transitions ) {
        if constexpr (I >= sizeof...(StateTrans)) {
          return;
        } else {
          std::cout << "Connecting connecting" << std::endl;
          auto &transition = std::get<I>( transitions );
          transition.eventSource ( &std::forward<State>(nS).wrappedState() ).connect( makeEventCallback< std::decay_t<State> >(transition));
        }
      }

    private:
      signal <void ( StateId )> _sigStateChanged;
      signal <void ()> _sigFinished;
      StateSet _state = InitialState();
  };

}

#endif // ZYPP_NG_BASE_STATEMACHINE_INCLUDED_H
