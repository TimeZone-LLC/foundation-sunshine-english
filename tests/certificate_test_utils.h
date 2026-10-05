/**
 * @file tests/certificate_test_utils.h
 * @brief Certificate fixtures shared by Sunshine security tests.
 */
#pragma once

#include <algorithm>

#include <openssl/x509v3.h>

#include <src/crypto.h>

namespace test_utils::certificates {
  inline crypto::creds_t
  generate_ca_credentials(const std::string_view common_name = "Sunshine Test Client CA") {
    auto credentials = crypto::gen_creds(common_name, 2048);
    auto certificate = crypto::x509(credentials.x509);
    auto private_key = crypto::pkey(credentials.pkey);

    util::safe_ptr<BASIC_CONSTRAINTS, BASIC_CONSTRAINTS_free> constraints { BASIC_CONSTRAINTS_new() };
    constraints->ca = 1;
    X509_add1_ext_i2d(certificate.get(), NID_basic_constraints, constraints.get(), 1, X509V3_ADD_DEFAULT);
    X509_sign(certificate.get(), private_key.get(), EVP_sha256());
    credentials.x509 = crypto::pem(certificate);
    return credentials;
  }

  inline crypto::creds_t
  generate_derived_leaf(const crypto::creds_t &issuer_credentials) {
    auto credentials = crypto::gen_creds("Sunshine Test Derived Client", 2048);
    auto certificate = crypto::x509(credentials.x509);
    auto issuer_certificate = crypto::x509(issuer_credentials.x509);
    auto issuer_private_key = crypto::pkey(issuer_credentials.pkey);

    X509_set_issuer_name(certificate.get(), X509_get_subject_name(issuer_certificate.get()));
    X509_sign(certificate.get(), issuer_private_key.get(), EVP_sha256());
    credentials.x509 = crypto::pem(certificate);
    return credentials;
  }
}  // namespace test_utils::certificates
