#include "pairing.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <nlohmann/json.hpp>

#include "src/config.h"
#include "src/crypto.h"
#include "src/file_handler.h"
#include "src/globals.h"
#include "src/httpcommon.h"
#include "src/logging.h"
#include "src/network.h"
#include "src/tray/system_tray.h"
#include "src/tray/tray_state.h"
#include "src/utility.h"
#include "src/uuid.h"

using namespace std::literals;

namespace nvhttp {

  void
  remove_session(pair_session_t &sess);

  bool
  getservercert_checked(pair_session_t &sess, boost::property_tree::ptree &tree, const std::string &pin, const std::string &client_name);

  void
  clientchallenge(pair_session_t &sess, boost::property_tree::ptree &tree, const std::string &challenge);

  void
  serverchallengeresp(pair_session_t &sess, boost::property_tree::ptree &tree, const std::string &encrypted_response);

  void
  clientpairingsecret(pair_session_t &sess, boost::property_tree::ptree &tree, const std::string &client_pairing_secret);

  void
  fail_pair(pair_session_t &sess, boost::property_tree::ptree &tree, std::string status_msg);

  std::string
  consume_preset_pin();

  namespace {

    namespace fs = std::filesystem;
    namespace pt = boost::property_tree;

    fs::path
    state_file_path() {
      return file_handler::path_from_utf8(config::nvhttp.file_state);
    }

    struct named_cert_t {
      std::string name;
      std::string uuid;
      std::string cert;
    };

    struct client_t {
      std::vector<named_cert_t> named_devices;
    };

    struct conf_intern_t {
      std::string servercert;
      std::string pkey;
    } conf_intern;

    struct pair_rate_limiter_t {
      std::unordered_map<std::string, std::pair<int, std::chrono::steady_clock::time_point>> attempts;
      std::mutex mutex;

      bool
      check_and_record(const std::string &ip) {
        const int max_attempts = config::nvhttp.pair_max_attempts;
        constexpr int window_seconds = 60;
        if (max_attempts <= 0) {
          return true;
        }

        std::lock_guard lock { mutex };
        auto now = std::chrono::steady_clock::now();
        auto &entry = attempts[ip];

        if (now - entry.second > std::chrono::seconds(window_seconds)) {
          entry = { 0, now };
        }

        if (entry.first >= max_attempts) {
          return false;
        }

        entry.first++;
        return true;
      }

      void
      cleanup() {
        constexpr int window_seconds = 60;
        std::lock_guard lock { mutex };
        auto now = std::chrono::steady_clock::now();
        for (auto it = attempts.begin(); it != attempts.end();) {
          if (now - it->second.second > std::chrono::seconds(window_seconds * 2)) {
            it = attempts.erase(it);
          }
          else {
            ++it;
          }
        }
      }
    };

    static crypto::cert_chain_t cert_chain;
    static std::unordered_map<std::string, pair_session_t> map_id_sess;
    static client_t client_root;
    static std::string last_pair_name;
    static std::string notified_pairing_id;
    static std::mutex map_id_sess_mutex;
    static std::shared_mutex client_state_mutex;
    static pair_rate_limiter_t pair_rate_limit;

    static struct {
      std::string pin;
      std::string name;
      std::chrono::steady_clock::time_point expires_at;
      bool paired = false;
      std::mutex mutex;
    } preset_pin_state;

    using args_t = SimpleWeb::CaseInsensitiveMultimap;

    std::string
    get_arg(const args_t &args, const char *name, const char *default_value = nullptr) {
      auto it = args.find(name);
      if (it == std::end(args)) {
        if (default_value != nullptr) {
          return std::string(default_value);
        }

        throw std::out_of_range(name);
      }
      return it->second;
    }

    template <class T>
    constexpr std::string_view
    tunnel_name() {
      if constexpr (std::is_same_v<T, SunshineHTTPS>) {
        return "HTTPS"sv;
      }
      return "NONE"sv;
    }

    template <class T>
    void
    log_request(std::shared_ptr<typename SimpleWeb::ServerBase<T>::Request> request) {
      BOOST_LOG(debug) << "Request - Protocol: " << tunnel_name<T>()
                       << ", IP: " << request->remote_endpoint().address().to_string()
                       << ", PORT: " << request->remote_endpoint().port()
                       << ", METHOD: " << request->method
                       << ", PATH: " << request->path;
    }

    void
    save_state(bool preserve_existing = true) {
      pt::ptree root;
      const auto state_path = state_file_path();

      if (preserve_existing) {
        try {
          if (fs::exists(state_path)) {
            std::ifstream state_file { state_path };
            if (!state_file.is_open()) {
              throw std::runtime_error("unable to open state file for reading");
            }
            pt::read_json(state_file, root);
          }
        }
        catch (std::exception &e) {
          BOOST_LOG(error) << "Couldn't read "sv << config::nvhttp.file_state << ": "sv << e.what();
          return;
        }
      }

      root.erase("root"s);

      root.put("root.uniqueid", http::unique_id);
      pt::ptree named_cert_nodes;
      {
        std::shared_lock<std::shared_mutex> cl(client_state_mutex);
        for (auto &named_cert : client_root.named_devices) {
          pt::ptree named_cert_node;
          named_cert_node.put("name"s, named_cert.name);
          named_cert_node.put("cert"s, named_cert.cert);
          named_cert_node.put("uuid"s, named_cert.uuid);
          named_cert_nodes.push_back(std::make_pair(""s, named_cert_node));
        }
      }
      root.add_child("root.named_devices"s, named_cert_nodes);

      try {
        std::ofstream state_file { state_path };
        if (!state_file.is_open()) {
          throw std::runtime_error("unable to open state file for writing");
        }
        pt::write_json(state_file, root);
        state_file.flush();
        if (!state_file) {
          throw std::runtime_error("unable to write state file");
        }
      }
      catch (std::exception &e) {
        BOOST_LOG(error) << "Couldn't write "sv << config::nvhttp.file_state << ": "sv << e.what();
        return;
      }
    }

    void
    rebuild_client_cert_chain() {
      cert_chain.clear();
      for (const auto &client : client_root.named_devices) {
        if (auto certificate = crypto::x509(client.cert)) {
          cert_chain.add(std::move(certificate));
        }
      }
    }

    std::string
    add_authorized_client(const std::string &name, std::string &&cert) {
      auto certificate = crypto::x509(cert);
      if (!certificate) {
        return {};
      }

      named_cert_t named_cert;
      named_cert.name = name;
      named_cert.cert = crypto::pem(certificate);
      named_cert.uuid = uuid_util::uuid_t::generate().string();
      const auto uuid = named_cert.uuid;
      {
        std::unique_lock<std::shared_mutex> ul(client_state_mutex);
        std::erase_if(client_root.named_devices, [&certificate](const named_cert_t &existing_client) {
          auto existing_certificate = crypto::x509(existing_client.cert);
          return existing_certificate && X509_cmp(existing_certificate.get(), certificate.get()) == 0;
        });
        client_root.named_devices.emplace_back(std::move(named_cert));
        rebuild_client_cert_chain();
      }

      if (!config::sunshine.flags[config::flag::FRESH_STATE]) {
        save_state();
      }
      return uuid;
    }

    void
    complete_pairing(pair_session_t &sess, const bool success) {
      {
        std::lock_guard lock { sess.completion->mutex };
        if (sess.completion->result.has_value()) {
          return;
        }
        sess.completion->result = success;
      }
      sess.completion->condition.notify_all();
    }

    bool
    write_pairing_response(pair_session_t &sess, const pt::ptree &tree) {
      std::ostringstream data;
      pt::write_xml(data, tree);

      auto &response = sess.async_insert_pin.response;
      if (response.has_left() && response.left()) {
        response.left()->close_connection_after_response = true;
        response.left()->write(data.str());
      }
      else if (response.has_right() && response.right()) {
        response.right()->close_connection_after_response = true;
        response.right()->write(data.str());
      }
      else {
        return false;
      }

      response = std::decay_t<decltype(response.left())>();
      return true;
    }

    void
    update_pairing_notification_unlocked() {
      const auto next = std::ranges::min_element(map_id_sess, {}, [](const auto &entry) {
        const auto &session = entry.second;
        return session.async_insert_pin.awaiting_approval ? session.async_insert_pin.expires_at : std::chrono::steady_clock::time_point::max();
      });

      if (next == map_id_sess.end() || !next->second.async_insert_pin.awaiting_approval) {
        if (!notified_pairing_id.empty()) {
          notified_pairing_id.clear();
          tray_state::clear_pairing_required();
        }
        return;
      }

      if (notified_pairing_id == next->second.async_insert_pin.id) {
        return;
      }

      notified_pairing_id = next->second.async_insert_pin.id;
      tray_state::set_pairing_required(next->second.async_insert_pin.device_name);
#if defined SUNSHINE_TRAY && SUNSHINE_TRAY >= 1 && !defined SUNSHINE_TESTS
      system_tray::update_tray_require_pin(next->second.async_insert_pin.device_name);
#endif
    }

    void
    expire_pair_sessions_unlocked(const std::chrono::steady_clock::time_point now) {
      for (auto it = map_id_sess.begin(); it != map_id_sess.end();) {
        if (it->second.async_insert_pin.expires_at > now) {
          ++it;
          continue;
        }

        pt::ptree tree;
        tree.put("root.paired", 0);
        tree.put("root.<xmlattr>.status_code", 408);
        tree.put("root.<xmlattr>.status_message", "Pairing session expired");
        write_pairing_response(it->second, tree);
        complete_pairing(it->second, false);
        it = map_id_sess.erase(it);
      }
    }

    template <class T>
    void
    pair_impl(std::shared_ptr<typename SimpleWeb::ServerBase<T>::Response> response, std::shared_ptr<typename SimpleWeb::ServerBase<T>::Request> request) {
      log_request<T>(request);

      auto client_ip = request->remote_endpoint().address().to_string();
      pair_rate_limit.cleanup();

      pt::ptree tree;
      auto fg = util::fail_guard([&]() {
        std::ostringstream data;

        pt::write_xml(data, tree);
        response->write(data.str());
        response->close_connection_after_response = true;
      });

      auto args = request->parse_query_string();
      if (args.find("uniqueid"s) == std::end(args)) {
        tree.put("root.<xmlattr>.status_code", 400);
        tree.put("root.<xmlattr>.status_message", "Missing uniqueid parameter");
        return;
      }

      auto uniqID { get_arg(args, "uniqueid") };

      args_t::const_iterator it;
      if (it = args.find("phrase"); it != std::end(args)) {
        if (it->second == "getservercert"sv) {
          if (!pair_rate_limit.check_and_record(client_ip)) {
            BOOST_LOG(warning) << "Pairing rate limited for IP: " << client_ip;
            tree.put("root.<xmlattr>.status_code", 429);
            tree.put("root.<xmlattr>.status_message", "Too many pairing attempts. Try again later.");
            tree.put("root.paired", 0);
            return;
          }

          pair_session_t sess;

          sess.client.uniqueID = uniqID;
          sess.client.cert = util::from_hex_vec(get_arg(args, "clientcert"), true);
          sess.async_insert_pin.salt = get_arg(args, "salt");
          sess.async_insert_pin.device_name = get_arg(args, "clientname", "Unnamed client");
          sess.async_insert_pin.address = net::addr_to_normalized_string(request->remote_endpoint().address());

          BOOST_LOG(verbose) << "Client cert: " << sess.client.cert.substr(0, 100) << "...";
          const auto device_name = sess.async_insert_pin.device_name;
          const bool pin_stdin = config::sunshine.flags[config::flag::PIN_STDIN];
          const auto remote_addr = request->remote_endpoint().address();
          const auto nettype = net::from_address(remote_addr.to_string());

          std::string pairing_id;
          switch (insert_pair_session(std::move(sess), pairing_id)) {
            case pair_session_insert_e::ALREADY_EXISTS:
              tree.put("root.paired", 0);
              tree.put("root.<xmlattr>.status_code", 409);
              tree.put("root.<xmlattr>.status_message", "A pairing session with this uniqueid already exists");
              return;
            case pair_session_insert_e::FULL:
              tree.put("root.paired", 0);
              tree.put("root.<xmlattr>.status_code", 503);
              tree.put("root.<xmlattr>.status_message", "Too many pending pairing sessions");
              return;
            case pair_session_insert_e::ENTROPY_FAILURE:
              tree.put("root.paired", 0);
              tree.put("root.<xmlattr>.status_code", 503);
              tree.put("root.<xmlattr>.status_message", "Unable to create a secure pairing request");
              return;
            case pair_session_insert_e::ADDED:
              break;
          }

          if (pin_stdin) {
            std::string pin;

            std::cout << "Please insert pin: "sv;
            std::getline(std::cin, pin);

            std::lock_guard map_lock { map_id_sess_mutex };
            expire_pair_sessions_unlocked(std::chrono::steady_clock::now());
            const auto session = map_id_sess.find(uniqID);
            if (session == map_id_sess.end() || session->second.async_insert_pin.id != pairing_id) {
              tree.put("root.paired", 0);
              tree.put("root.<xmlattr>.status_code", 408);
              tree.put("root.<xmlattr>.status_message", "Pairing session expired");
              update_pairing_notification_unlocked();
              return;
            }
            session->second.client.name = device_name;
            getservercert_checked(session->second, tree, pin, device_name);
            if (session->second.failed) {
              map_id_sess.erase(session);
            }
            update_pairing_notification_unlocked();
            return;
          }

          auto preset = (nettype == net::net_e::PC || nettype == net::net_e::LAN) ? consume_preset_pin() : std::string {};
          if (!preset.empty()) {
            BOOST_LOG(info) << "Using preset PIN for QR code pairing with " << device_name
                            << " from " << remote_addr.to_string();
            std::lock_guard map_lock { map_id_sess_mutex };
            expire_pair_sessions_unlocked(std::chrono::steady_clock::now());
            const auto session = map_id_sess.find(uniqID);
            if (session == map_id_sess.end() || session->second.async_insert_pin.id != pairing_id) {
              tree.put("root.paired", 0);
              tree.put("root.<xmlattr>.status_code", 408);
              tree.put("root.<xmlattr>.status_message", "Pairing session expired");
              update_pairing_notification_unlocked();
              return;
            }
            session->second.client.name = device_name;
            getservercert_checked(session->second, tree, preset, device_name);
            if (session->second.failed) {
              map_id_sess.erase(session);
            }
            update_pairing_notification_unlocked();
            return;
          }

          {
            std::lock_guard map_lock { map_id_sess_mutex };
            const auto session = map_id_sess.find(uniqID);
            if (session == map_id_sess.end() || session->second.async_insert_pin.id != pairing_id) {
              tree.put("root.paired", 0);
              tree.put("root.<xmlattr>.status_code", 408);
              tree.put("root.<xmlattr>.status_message", "Pairing session expired");
              update_pairing_notification_unlocked();
              return;
            }
            last_pair_name = device_name;
            session->second.async_insert_pin.response = response;
            session->second.async_insert_pin.awaiting_approval = true;
            update_pairing_notification_unlocked();
          }

          task_pool.pushDelayed([]() {
            expire_pair_sessions();
          }, PAIRING_SESSION_TIMEOUT);

          fg.disable();
          return;
        }
        else if (it->second == "pairchallenge"sv) {
          tree.put("root.paired", 1);
          tree.put("root.<xmlattr>.status_code", 200);
          return;
        }
      }

      std::lock_guard map_lock { map_id_sess_mutex };
      expire_pair_sessions_unlocked(std::chrono::steady_clock::now());
      auto sess_it = map_id_sess.find(uniqID);
      if (sess_it == std::end(map_id_sess)) {
        tree.put("root.<xmlattr>.status_code", 400);
        tree.put("root.<xmlattr>.status_message", "Invalid uniqueid");
        update_pairing_notification_unlocked();
        return;
      }

      bool pairing_complete = false;
      if (it = args.find("clientchallenge"); it != std::end(args)) {
        auto challenge = util::from_hex_vec(it->second, true);
        clientchallenge(sess_it->second, tree, challenge);
      }
      else if (it = args.find("serverchallengeresp"); it != std::end(args)) {
        auto encrypted_response = util::from_hex_vec(it->second, true);
        serverchallengeresp(sess_it->second, tree, encrypted_response);
      }
      else if (it = args.find("clientpairingsecret"); it != std::end(args)) {
        auto pairingsecret = util::from_hex_vec(it->second, true);
        clientpairingsecret(sess_it->second, tree, pairingsecret);
        pairing_complete = true;
      }
      else {
        fail_pair(sess_it->second, tree, "Invalid pairing request");
      }

      if (pairing_complete || sess_it->second.failed) {
        map_id_sess.erase(sess_it);
      }
      update_pairing_notification_unlocked();
    }

  }  // namespace

  bool
  is_valid_pairing_id(const std::string_view pairing_id) {
    return pairing_id.size() == PAIRING_ID_SIZE && std::ranges::all_of(pairing_id, [](const char character) {
             return (character >= '0' && character <= '9') ||
                    (character >= 'a' && character <= 'f') ||
                    (character >= 'A' && character <= 'F');
           });
  }

  bool
  is_valid_pairing_pin(const std::string_view pin) {
    return pin.size() == 4 && std::ranges::all_of(pin, [](const char character) {
             return character >= '0' && character <= '9';
           });
  }

  bool
  is_valid_pairing_name(const std::string_view name) {
    return !name.empty() && name.size() <= MAX_PAIRING_CLIENT_NAME_SIZE;
  }

  pair_session_insert_e
  insert_pair_session(pair_session_t sess, std::string &pairing_id) {
    std::lock_guard lock { map_id_sess_mutex };
    expire_pair_sessions_unlocked(std::chrono::steady_clock::now());
    pairing_id.clear();

    if (map_id_sess.contains(sess.client.uniqueID)) {
      update_pairing_notification_unlocked();
      return pair_session_insert_e::ALREADY_EXISTS;
    }
    if (map_id_sess.size() >= MAX_PENDING_PAIRING_SESSIONS) {
      update_pairing_notification_unlocked();
      return pair_session_insert_e::FULL;
    }

    constexpr int max_id_attempts = 8;
    for (int attempt = 0; attempt < max_id_attempts; ++attempt) {
      const auto random_bytes = crypto::rand(PAIRING_ID_SIZE / 2);
      if (random_bytes.size() != PAIRING_ID_SIZE / 2) {
        return pair_session_insert_e::ENTROPY_FAILURE;
      }

      pairing_id = util::hex_vec(random_bytes, true);
      const bool collision = std::ranges::any_of(map_id_sess, [&pairing_id](const auto &entry) {
        return entry.second.async_insert_pin.id == pairing_id;
      });
      if (!collision) {
        sess.async_insert_pin.id = pairing_id;
        sess.async_insert_pin.expires_at = std::chrono::steady_clock::now() + PAIRING_SESSION_TIMEOUT;
        map_id_sess.emplace(sess.client.uniqueID, std::move(sess));
        return pair_session_insert_e::ADDED;
      }
    }

    pairing_id.clear();
    return pair_session_insert_e::ENTROPY_FAILURE;
  }

  void
  expire_pair_sessions(const std::chrono::steady_clock::time_point now) {
    std::lock_guard lock { map_id_sess_mutex };
    expire_pair_sessions_unlocked(now);
    update_pairing_notification_unlocked();
  }

  std::vector<pending_pairing_t>
  get_pending_pairings() {
    std::lock_guard lock { map_id_sess_mutex };
    expire_pair_sessions_unlocked(std::chrono::steady_clock::now());

    std::vector<const pair_session_t *> pending_sessions;
    pending_sessions.reserve(map_id_sess.size());
    for (const auto &[_, session] : map_id_sess) {
      if (session.async_insert_pin.awaiting_approval && session.last_phase == PAIR_PHASE::NONE) {
        pending_sessions.push_back(&session);
      }
    }
    std::ranges::sort(pending_sessions, {}, [](const pair_session_t *session) {
      return session->async_insert_pin.expires_at;
    });

    std::vector<pending_pairing_t> result;
    result.reserve(pending_sessions.size());
    for (const auto *session : pending_sessions) {
      result.push_back({
        .id = session->async_insert_pin.id,
        .name = session->async_insert_pin.device_name,
        .address = session->async_insert_pin.address,
      });
    }
    update_pairing_notification_unlocked();
    return result;
  }

  bool
  cancel_pairing(const std::string_view pairing_id) {
    if (!is_valid_pairing_id(pairing_id)) {
      return false;
    }

    std::lock_guard lock { map_id_sess_mutex };
    expire_pair_sessions_unlocked(std::chrono::steady_clock::now());
    const auto session = std::ranges::find_if(map_id_sess, [pairing_id](const auto &entry) {
      return entry.second.async_insert_pin.awaiting_approval &&
             entry.second.last_phase == PAIR_PHASE::NONE &&
             entry.second.async_insert_pin.id == pairing_id;
    });
    if (session == map_id_sess.end()) {
      update_pairing_notification_unlocked();
      return false;
    }

    pt::ptree tree;
    tree.put("root.paired", 0);
    tree.put("root.<xmlattr>.status_code", 400);
    tree.put("root.<xmlattr>.status_message", "Pairing request cancelled by operator");
    write_pairing_response(session->second, tree);
    complete_pairing(session->second, false);
    map_id_sess.erase(session);
    update_pairing_notification_unlocked();
    return true;
  }

  namespace pairing {

    void
    set_credentials(const std::string &pkey, const std::string &cert) {
      conf_intern.pkey = pkey;
      conf_intern.servercert = cert;
    }

    void
    load_state() {
      const auto state_path = state_file_path();
      pt::ptree tree;
      try {
        if (!fs::exists(state_path)) {
          BOOST_LOG(debug) << "File "sv << config::nvhttp.file_state << " doesn't exist"sv;
          http::unique_id = uuid_util::uuid_t::generate().string();
          return;
        }
        std::ifstream state_file { state_path };
        if (!state_file.is_open()) {
          throw std::runtime_error("unable to open state file for reading");
        }
        pt::read_json(state_file, tree);
      }
      catch (std::exception &e) {
        BOOST_LOG(error) << "Couldn't read "sv << config::nvhttp.file_state << ": "sv << e.what();
        http::unique_id = uuid_util::uuid_t::generate().string();
        {
          std::unique_lock<std::shared_mutex> ul(client_state_mutex);
          cert_chain.clear();
          client_root = client_t {};
        }
        save_state(false);
        return;
      }

      auto unique_id_p = tree.get_optional<std::string>("root.uniqueid");
      if (!unique_id_p) {
        http::unique_id = uuid_util::uuid_t::generate().string();
        return;
      }
      http::unique_id = std::move(*unique_id_p);

      auto root = tree.get_child("root");
      client_t client;

      if (root.get_child_optional("devices")) {
        auto device_nodes = root.get_child("devices");
        for (auto &[_, device_node] : device_nodes) {
          if (device_node.count("certs")) {
            for (auto &[_, el] : device_node.get_child("certs")) {
              named_cert_t named_cert;
              named_cert.name = ""s;
              named_cert.cert = el.get_value<std::string>();
              named_cert.uuid = uuid_util::uuid_t::generate().string();
              client.named_devices.emplace_back(named_cert);
            }
          }
        }
      }

      if (root.count("named_devices")) {
        for (auto &[_, el] : root.get_child("named_devices")) {
          named_cert_t named_cert;
          named_cert.name = el.get_child("name").get_value<std::string>();
          named_cert.cert = el.get_child("cert").get_value<std::string>();
          named_cert.uuid = el.get_child("uuid").get_value<std::string>();
          client.named_devices.emplace_back(named_cert);
        }
      }

      {
        std::unique_lock<std::shared_mutex> ul(client_state_mutex);
        client_root = std::move(client);
        rebuild_client_cert_chain();
      }
    }

    std::string
    client_uuid_for_cert(const X509 *cert) {
      if (!cert) {
        return {};
      }

      std::shared_lock<std::shared_mutex> cl(client_state_mutex);
      for (const auto &named_cert : client_root.named_devices) {
        if (crypto::x509_matches_pem(cert, named_cert.cert)) {
          return named_cert.uuid;
        }
      }
      return {};
    }

    const char *
    verify_client_certificate(X509 *cert, bool close_verify_safe) {
      std::shared_lock<std::shared_mutex> sl(client_state_mutex);
      if (!close_verify_safe) {
        return cert_chain.verify_safe(cert);
      }
      return cert_chain.verify(cert);
    }

    void
    pair_https(resp_https_t response, req_https_t request) {
      pair_impl<SunshineHTTPS>(std::move(response), std::move(request));
    }

    void
    pair_http(resp_http_t response, req_http_t request) {
      pair_impl<SimpleWeb::HTTP>(std::move(response), std::move(request));
    }

#ifdef SUNSHINE_TESTS
    namespace test_support {
      std::string
      add_authorized_client(const std::string &name, std::string cert) {
        return ::nvhttp::add_authorized_client(name, std::move(cert));
      }

      void
      complete_pairing(pair_session_t &session, const bool success) {
        ::nvhttp::complete_pairing(session, success);
      }
    }  // namespace test_support
#endif

  }  // namespace pairing

  void
  remove_session(pair_session_t &sess) {
    complete_pairing(sess, false);
    std::lock_guard lock { map_id_sess_mutex };
    const auto session = map_id_sess.find(sess.client.uniqueID);
    if (session != map_id_sess.end() && &session->second == &sess) {
      map_id_sess.erase(session);
    }
    update_pairing_notification_unlocked();
  }

  void
  fail_pair(pair_session_t &sess, pt::ptree &tree, std::string status_msg) {
    tree.put("root.paired", 0);
    tree.put("root.<xmlattr>.status_code", 400);
    tree.put("root.<xmlattr>.status_message", std::move(status_msg));
    sess.failed = true;
    complete_pairing(sess, false);
  }

  bool
  getservercert_checked(pair_session_t &sess, pt::ptree &tree, const std::string &pin, const std::string &client_name) {
    if (sess.last_phase != PAIR_PHASE::NONE) {
      fail_pair(sess, tree, "Out of order call to getservercert");
      return false;
    }
    sess.last_phase = PAIR_PHASE::GETSERVERCERT;

    if (sess.async_insert_pin.salt.size() < 32) {
      fail_pair(sess, tree, "Salt too short");
      return false;
    }

    std::string_view salt_view { sess.async_insert_pin.salt.data(), 32 };

    auto salt = util::from_hex<std::array<uint8_t, 16>>(salt_view, true);

    auto key = crypto::gen_aes_key(salt, pin);
    sess.cipher_key = std::make_unique<crypto::aes_t>(key);

    tree.put("root.paired", 1);
    tree.put("root.pairname", client_name);
    tree.put("root.plaincert", util::hex_vec(conf_intern.servercert, true));
    tree.put("root.<xmlattr>.status_code", 200);
    return true;
  }

  void
  getservercert(pair_session_t &sess, pt::ptree &tree, const std::string &pin, const std::string &client_name) {
    (void) getservercert_checked(sess, tree, pin, client_name);
  }

  void
  clientchallenge(pair_session_t &sess, pt::ptree &tree, const std::string &challenge) {
    if (sess.last_phase != PAIR_PHASE::GETSERVERCERT) {
      fail_pair(sess, tree, "Out of order call to clientchallenge");
      return;
    }
    sess.last_phase = PAIR_PHASE::CLIENTCHALLENGE;

    if (!sess.cipher_key) {
      fail_pair(sess, tree, "Cipher key not set");
      return;
    }

    crypto::cipher::ecb_t cipher(*sess.cipher_key, false);

    std::vector<uint8_t> decrypted;
    cipher.decrypt(challenge, decrypted);

    auto x509 = crypto::x509(conf_intern.servercert);
    auto sign = crypto::signature(x509);
    auto serversecret = crypto::rand(16);
    auto serverchallenge = crypto::rand(16);
    if (serversecret.size() != 16 || serverchallenge.size() != 16) {
      fail_pair(sess, tree, "Failed to generate pairing challenge");
      return;
    }

    decrypted.insert(std::end(decrypted), std::begin(sign), std::end(sign));
    decrypted.insert(std::end(decrypted), std::begin(serversecret), std::end(serversecret));

    auto hash = crypto::hash({ (char *) decrypted.data(), decrypted.size() });

    std::string plaintext;
    plaintext.reserve(hash.size() + serverchallenge.size());

    plaintext.insert(std::end(plaintext), std::begin(hash), std::end(hash));
    plaintext.insert(std::end(plaintext), std::begin(serverchallenge), std::end(serverchallenge));

    std::vector<uint8_t> encrypted;
    cipher.encrypt(plaintext, encrypted);

    sess.serversecret = std::move(serversecret);
    sess.serverchallenge = std::move(serverchallenge);

    tree.put("root.paired", 1);
    tree.put("root.challengeresponse", util::hex_vec(encrypted, true));
    tree.put("root.<xmlattr>.status_code", 200);
  }

  void
  serverchallengeresp(pair_session_t &sess, pt::ptree &tree, const std::string &encrypted_response) {
    if (sess.last_phase != PAIR_PHASE::CLIENTCHALLENGE) {
      fail_pair(sess, tree, "Out of order call to serverchallengeresp");
      return;
    }
    sess.last_phase = PAIR_PHASE::SERVERCHALLENGERESP;

    if (!sess.cipher_key || sess.serversecret.empty()) {
      fail_pair(sess, tree, "Cipher key or serversecret not set");
      return;
    }

    std::vector<uint8_t> decrypted;
    crypto::cipher::ecb_t cipher(*sess.cipher_key, false);

    cipher.decrypt(encrypted_response, decrypted);

    sess.clienthash = std::move(decrypted);

    auto serversecret = sess.serversecret;
    auto sign = crypto::sign256(crypto::pkey(conf_intern.pkey), serversecret);

    serversecret.insert(std::end(serversecret), std::begin(sign), std::end(sign));

    tree.put("root.pairingsecret", util::hex_vec(serversecret, true));
    tree.put("root.paired", 1);
    tree.put("root.<xmlattr>.status_code", 200);
  }

  void
  clientpairingsecret(pair_session_t &sess, pt::ptree &tree, const std::string &client_pairing_secret) {
    if (sess.last_phase != PAIR_PHASE::SERVERCHALLENGERESP) {
      fail_pair(sess, tree, "Out of order call to clientpairingsecret");
      return;
    }
    sess.last_phase = PAIR_PHASE::CLIENTPAIRINGSECRET;

    auto &client = sess.client;

    if (client_pairing_secret.size() <= 16) {
      fail_pair(sess, tree, "Client pairing secret too short");
      return;
    }

    std::string_view secret { client_pairing_secret.data(), 16 };
    std::string_view sign { client_pairing_secret.data() + secret.size(), client_pairing_secret.size() - secret.size() };

    auto x509 = crypto::x509(client.cert);
    if (!x509) {
      fail_pair(sess, tree, "Invalid client certificate");
      return;
    }
    auto x509_sign = crypto::signature(x509);

    std::string data;
    data.reserve(sess.serverchallenge.size() + x509_sign.size() + secret.size());

    data.insert(std::end(data), std::begin(sess.serverchallenge), std::end(sess.serverchallenge));
    data.insert(std::end(data), std::begin(x509_sign), std::end(x509_sign));
    data.insert(std::end(data), std::begin(secret), std::end(secret));

    auto hash = crypto::hash(data);

    const bool same_hash = hash.size() == sess.clienthash.size() && std::equal(hash.begin(), hash.end(), sess.clienthash.begin());
    const bool verify = crypto::verify256(crypto::x509(client.cert), secret, sign);
    bool paired = false;
    if (same_hash && verify) {
      paired = !add_authorized_client(client.name, std::move(client.cert)).empty();
    }

    tree.put("root.paired", paired ? 1 : 0);
    complete_pairing(sess, paired);
    tree.put("root.<xmlattr>.status_code", 200);
  }

  void
  clientpairingsecret(pair_session_t &sess, std::shared_ptr<safe::queue_t<crypto::x509_t>> & /*add_cert*/, pt::ptree &tree, const std::string &client_pairing_secret) {
    clientpairingsecret(sess, tree, client_pairing_secret);
  }

  bool
  pin(const std::string_view pairing_id, std::string pin, std::string name) {
    if (!is_valid_pairing_id(pairing_id) || !is_valid_pairing_pin(pin) || !is_valid_pairing_name(name)) {
      return false;
    }

    std::shared_ptr<pairing_completion_t> completion;
    std::string session_id;
    {
      std::lock_guard map_lock { map_id_sess_mutex };
      expire_pair_sessions_unlocked(std::chrono::steady_clock::now());
      const auto sess_it = std::ranges::find_if(map_id_sess, [pairing_id](const auto &entry) {
        return entry.second.async_insert_pin.awaiting_approval &&
               entry.second.last_phase == PAIR_PHASE::NONE &&
               entry.second.async_insert_pin.id == pairing_id;
      });
      if (sess_it == map_id_sess.end()) {
        BOOST_LOG(warning) << "Cannot apply PIN because the pending pairing session is no longer available";
        update_pairing_notification_unlocked();
        return false;
      }

      auto &sess = sess_it->second;
      sess.async_insert_pin.awaiting_approval = false;
      sess.client.name = std::move(name);
      pt::ptree tree;
      getservercert_checked(sess, tree, pin, sess.client.name);
      const bool response_written = write_pairing_response(sess, tree);
      if (!response_written || sess.failed) {
        complete_pairing(sess, false);
        map_id_sess.erase(sess_it);
        update_pairing_notification_unlocked();
        return false;
      }

      completion = sess.completion;
      session_id = sess.client.uniqueID;
      update_pairing_notification_unlocked();
    }

    {
      std::unique_lock completion_lock { completion->mutex };
      if (completion->condition.wait_for(completion_lock, config::stream.ping_timeout, [&completion]() {
            return completion->result.has_value();
          })) {
        return *completion->result;
      }
    }

    std::lock_guard map_lock { map_id_sess_mutex };
    if (const auto sess_it = map_id_sess.find(session_id);
        sess_it != map_id_sess.end() && sess_it->second.completion == completion) {
      complete_pairing(sess_it->second, false);
      map_id_sess.erase(sess_it);
    }
    update_pairing_notification_unlocked();
    return false;
  }

  nlohmann::json
  get_all_clients() {
    nlohmann::json named_cert_nodes = nlohmann::json::array();
    std::shared_lock<std::shared_mutex> cl(client_state_mutex);
    for (auto &named_cert : client_root.named_devices) {
      nlohmann::json named_cert_node;
      named_cert_node["name"] = named_cert.name;
      named_cert_node["uuid"] = named_cert.uuid;
      named_cert_nodes.push_back(named_cert_node);
    }

    return named_cert_nodes;
  }

  std::string
  get_pair_name() {
    std::lock_guard<std::mutex> map_lock(map_id_sess_mutex);
    return last_pair_name;
  }

  bool
  set_preset_pin(const std::string &pin, const std::string &name, int timeout_seconds) {
    if (pin.size() != 4 || !std::all_of(pin.begin(), pin.end(), ::isdigit)) {
      BOOST_LOG(warning) << "Invalid preset PIN: must be 4 digits";
      return false;
    }

    std::lock_guard lock { preset_pin_state.mutex };
    preset_pin_state.pin = pin;
    preset_pin_state.name = name;
    preset_pin_state.expires_at = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
    preset_pin_state.paired = false;
    BOOST_LOG(info) << "Preset PIN set for QR pairing, expires in " << timeout_seconds << "s";
    return true;
  }

  std::string
  consume_preset_pin() {
    std::lock_guard lock { preset_pin_state.mutex };
    if (preset_pin_state.pin.empty()) {
      return std::string {};
    }
    if (std::chrono::steady_clock::now() > preset_pin_state.expires_at) {
      preset_pin_state.pin.clear();
      preset_pin_state.name.clear();
      return std::string {};
    }
    std::string pin = std::move(preset_pin_state.pin);
    preset_pin_state.pin.clear();
    preset_pin_state.name.clear();
    return pin;
  }

  void
  clear_preset_pin() {
    std::lock_guard lock { preset_pin_state.mutex };
    preset_pin_state.pin.clear();
    preset_pin_state.name.clear();
    preset_pin_state.paired = true;
  }

  std::string
  get_qr_pair_status() {
    std::lock_guard lock { preset_pin_state.mutex };
    if (preset_pin_state.paired) return "paired";
    if (preset_pin_state.pin.empty()) return "inactive";
    if (std::chrono::steady_clock::now() > preset_pin_state.expires_at) {
      preset_pin_state.pin.clear();
      preset_pin_state.name.clear();
      return "expired";
    }
    return "active";
  }

  void
  erase_all_clients() {
    {
      std::unique_lock<std::shared_mutex> ul(client_state_mutex);
      client_root = client_t {};
      cert_chain.clear();
    }
    save_state();
  }

  int
  unpair_client(std::string uuid) {
    bool removed = false;
    {
      std::unique_lock<std::shared_mutex> ul(client_state_mutex);
      auto &devices = client_root.named_devices;
      for (auto it = devices.begin(); it != devices.end();) {
        if (it->uuid == uuid) {
          it = devices.erase(it);
          removed = true;
        }
        else {
          ++it;
        }
      }
    }

    save_state();
    pairing::load_state();
    return removed;
  }

  bool
  rename_client(const std::string &uuid, const std::string &new_name) {
    bool renamed = false;
    {
      std::unique_lock<std::shared_mutex> ul(client_state_mutex);
      for (auto &named_cert : client_root.named_devices) {
        if (named_cert.uuid == uuid) {
          named_cert.name = new_name;
          renamed = true;
          break;
        }
      }
    }
    if (renamed) {
      save_state();
    }
    return renamed;
  }

}  // namespace nvhttp
