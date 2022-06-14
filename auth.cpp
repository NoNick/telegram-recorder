#include <unistd.h>

#include <libconfig.h++>

#include "telegram_recorder.hpp"

auto TelegramRecorder::createAuthQueryHandler() {
  return [this, id = this->authQueryID](TDAPIObjectPtr object) {
    if(id == authQueryID) {
      checkAuthError(std::move(object));
    }
  };
}

void TelegramRecorder::onAuthStateUpdate() {
  ++this->authQueryID;
  td_api::downcast_call(
    *this->authState,
    overload {
      [this](td_api::authorizationStateReady&) {
        this->authorized = true;
        std::cout << "Got authorization" << std::endl;
      },
      [this](td_api::authorizationStateLoggingOut&) {
        this->authorized = false;
        std::cout << "Logging out" << std::endl;
      },
      [this](td_api::authorizationStateClosing&) {
        std::cout << "Closing" << std::endl;
      },
      [this](td_api::authorizationStateClosed&) {
        this->authorized = false;
        this->needRestart = true;
        std::cout << "Terminated (Needs restart)" << std::endl;
      },
      [this](td_api::authorizationStateWaitCode&) {
        std::cout << "Enter authentication code: " << std::flush;
        std::string code;
        std::getline(std::cin, code);
        this->sendQuery(
          td_api::make_object<td_api::checkAuthenticationCode>(code),
          this->createAuthQueryHandler()
        );
      },
      [this](td_api::authorizationStateWaitRegistration &) {
        this->sendQuery(
          td_api::make_object<td_api::registerUser>(this->firstName, this->lastName),
          this->createAuthQueryHandler()
        );
      },
      [this](td_api::authorizationStateWaitPassword&) {
        std::cout << "Enter authentication password: " << std::flush;
        std::string password;
        std::getline(std::cin, password);
        this->sendQuery(
          td_api::make_object<td_api::checkAuthenticationPassword>(password),
          this->createAuthQueryHandler()
        );
      },
      [this](
        td_api::authorizationStateWaitOtherDeviceConfirmation& state) {
          std::cout << "Confirm this login link on another device: "
                    << state.link_ << std::endl;
      },
      [this](td_api::authorizationStateWaitPhoneNumber&) {
        std::cout << "Enter phone number: " << std::flush;
        std::string phoneNumber;
        std::getline(std::cin, phoneNumber);
        this->sendQuery(
          td_api::make_object<td_api::setAuthenticationPhoneNumber>(
              phoneNumber, 
              nullptr
          ),
          this->createAuthQueryHandler()
        );
      },
      [this](td_api::authorizationStateWaitEncryptionKey&) {
        // Default to an empty key (at this point who cares lmao)
        this->sendQuery(
          td_api::make_object<td_api::checkDatabaseEncryptionKey>(""),
          this->createAuthQueryHandler()
        );
      },
      [this](td_api::authorizationStateWaitTdlibParameters&) {
        auto params = td_api::make_object<td_api::tdlibParameters>();
        params->database_directory_ = "tdlib";
        params->use_message_database_ = true;
        params->use_secret_chats_ = true;
        params->api_id_ = this->apiID;
        params->api_hash_ = this->apiHash;
        params->system_language_code_ = "en";
        params->device_model_ = "Desktop";
        params->application_version_ = "1.0";
        params->enable_storage_optimizer_ = true;
        this->sendQuery(
          td_api::make_object<td_api::setTdlibParameters>(std::move(params)),
          this->createAuthQueryHandler()
        );
      }
    }
  );
}

void TelegramRecorder::checkAuthError(TDAPIObjectPtr object) {
  if (object->get_id() == td_api::error::ID) {
    auto error = td::move_tl_object_as<td_api::error>(object);
    std::cout << "Error: " << to_string(error) << std::flush;
    this->onAuthStateUpdate();
  }
}