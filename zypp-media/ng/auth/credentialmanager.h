/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp-media/ng/auth/CredentialManager
 *
 */
#ifndef ZYPP_MEDIA_NG_AUTH_CREDENTIALMANAGER_H
#define ZYPP_MEDIA_NG_AUTH_CREDENTIALMANAGER_H

#include <set>
#include <zypp-core/zyppng/base/base.h>
#include <zypp-media/auth/authdata.h>
#include <zypp-media/auth/credentialmanager.h>

namespace zyppng {

  namespace media {

    ZYPP_FWD_DECL_TYPE_WITH_REFS (CredentialManager);

    class CredentialManager : public Base
    {
      ZYPP_ADD_CREATE_FUNC (CredentialManager)
    public:
      using CredentialSet = std::set<zypp::media::AuthData_Ptr, zypp::media::AuthDataComparator>;
      using CredentialSize = CredentialSet::size_type;
      using CredentialIterator = CredentialSet::const_iterator;

      CredentialManager( const CredentialManager &) = delete;
      CredentialManager( CredentialManager &&) = delete;
      CredentialManager &operator=(const CredentialManager &) = delete;
      CredentialManager &operator=(CredentialManager &&) = delete;
      CredentialManager( ZYPP_PRIVATE_CONSTR_ARG, zypp::media::CredManagerSettings opts);

      ~CredentialManager() override;

    public:
      /**
       * Get credentials for the specified \a url.
       *
       * If the URL contains also username, it will be used to find the match
       * for this user (in case mutliple are available).
       *
       * \param url URL to find credentials for.
       * \return Pointer to retrieved authentication data on success or an empty
       *         AuthData_Ptr otherwise.
       * \todo return a copy instead?
       */
      zypp::media::AuthData_Ptr getCred(const zypp::Url & url);

      /**
       * Read credentials from a file.
       */
      zypp::media::AuthData_Ptr getCredFromFile(const zypp::Pathname & file);

      /**
       * Add new global credentials.
       */
      void addGlobalCred(const zypp::media::AuthData & cred);

      /**
       * Add new user credentials.
       */
      void addUserCred(const zypp::media::AuthData & cred);

      /**
       * Add new credentials with user callbacks.
       *
       * If the cred->url() contains 'credentials' query parameter, the
       * credentials will be automatically saved to the specified file using the
       * \ref saveInFile() method.
       *
       * Otherwise a callback will be called asking whether to save to custom
       * file, or to global or user's credentials catalog.
       *
       * \todo Currently no callback is called, credentials are automatically
       *       saved to user's credentials.cat if no 'credentials' parameter
       *       has been specified
       */
      void addCred(const zypp::media::AuthData & cred);

      /**
       * Saves any unsaved credentials added via \ref addUserCred() or
       * \a addGlobalCred() methods.
       */
      void save();

      /**
       * Saves given \a cred to global credentials file.
       *
       * \note Use this method to add just one piece of credentials. To add
       *       multiple items at once, use addGlobalCred() followed
       *       by save()
       */
      void saveInGlobal(const zypp::media::AuthData & cred);

      /**
       * Saves given \a cred to user's credentials file.
       *
       * \note Use this method to add just one piece of credentials. To add
       *       multiple items at once, use addUserCred() followed
       *       by save()
       */
      void saveInUser(const zypp::media::AuthData & cred);

      /**
       * Saves given \a cred to user specified credentials file.
       *
       * If the credFile path is absolute, it will be saved at that precise
       * location. If \a credFile is just a filename, it will be saved
       * in \ref CredManagerOptions::customCredFileDir. Otherwise the current
       * working directory will be prepended to the file path.
       */
      void saveInFile(const zypp::media::AuthData &, const zypp::Pathname & credFile);

      /**
       * Remove all global or user credentials from memory and disk.
       *
       * \param global  Whether to remove global or user credentials.
       */
      void clearAll(bool global = false);

      /*!
       * Helper function to find a matching AuthData instance in a CredentialSet
       */
      static zypp::media::AuthData_Ptr findIn(const CredentialManager::CredentialSet & set,  const zypp::Url & url, zypp::url::ViewOption vopt );

      /*!
       * Returns the timestamp of the database the given URL creds would be stored
       */
      time_t timestampForCredDatabase ( const zypp::Url &url );

      CredentialIterator credsGlobalBegin() const;
      CredentialIterator credsGlobalEnd()   const;
      CredentialSize     credsGlobalSize()  const;
      bool               credsGlobalEmpty() const;

      CredentialIterator credsUserBegin() const;
      CredentialIterator credsUserEnd()   const;
      CredentialSize     credsUserSize()  const;
      bool               credsUserEmpty() const;

      struct Impl;
    private:
      void init_globalCredentials();
      void init_userCredentials();

      bool processCredentials(zypp::media::AuthData_Ptr & cred);

      zypp::media::AuthData_Ptr getCredFromUrl(const zypp::Url & url) const;
      void saveGlobalCredentials();
      void saveUserCredentials();

      zypp::media::CredManagerSettings _options;

      CredentialSet _credsGlobal;
      CredentialSet _credsUser;
      CredentialSet _credsTmp;

      bool _globalDirty;
      bool _userDirty;
    };
  }
}


#endif