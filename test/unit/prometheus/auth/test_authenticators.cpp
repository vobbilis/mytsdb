#include <gtest/gtest.h>
#include "tsdb/prometheus/auth/no_auth.h"
#include "tsdb/prometheus/auth/basic_auth.h"
#include "tsdb/prometheus/auth/bearer_auth.h"
#include "tsdb/prometheus/auth/header_auth.h"
#include "tsdb/prometheus/auth/composite_auth.h"

using namespace tsdb::prometheus;
using namespace tsdb::prometheus::auth;

// Helper to create a request with headers
Request CreateRequest(const std::map<std::string, std::string>& headers) {
    Request req;
    req.method = "POST";
    req.path = "/api/v1/write";
    req.headers = headers;
    return req;
}

// ============================================================================
// NoAuthenticator Tests
// ============================================================================

TEST(NoAuthenticatorTest, AlwaysSucceeds) {
    NoAuthenticator auth;
    
    Request req = CreateRequest({});
    AuthResult result = auth.Authenticate(req);
    
    EXPECT_TRUE(result.authenticated);
    EXPECT_FALSE(result.tenant_id.has_value());
    EXPECT_TRUE(result.error.empty());
}

TEST(NoAuthenticatorTest, SucceedsWithAnyHeaders) {
    NoAuthenticator auth;
    
    Request req = CreateRequest({
        {"Authorization", "Bearer invalid"},
        {"X-Custom", "value"}
    });
    AuthResult result = auth.Authenticate(req);
    
    EXPECT_TRUE(result.authenticated);
}

// ============================================================================
// BasicAuthenticator Tests
// ============================================================================

TEST(BasicAuthenticatorTest, ValidCredentials) {
    BasicAuthenticator auth;
    auth.AddUserWithPassword("testuser", "testpass", "tenant1");
    
    // Base64("testuser:testpass") = "dGVzdHVzZXI6dGVzdHBhc3M="
    Request req = CreateRequest({
        {"Authorization", "Basic dGVzdHVzZXI6dGVzdHBhc3M="}
    });
    
    AuthResult result = auth.Authenticate(req);
    
    EXPECT_TRUE(result.authenticated);
    EXPECT_TRUE(result.tenant_id.has_value());
    EXPECT_EQ(result.tenant_id.value(), "tenant1");
}

TEST(BasicAuthenticatorTest, InvalidPassword) {
    BasicAuthenticator auth;
    auth.AddUserWithPassword("testuser", "correctpass");
    
    // Base64("testuser:wrongpass") = "dGVzdHVzZXI6d3JvbmdwYXNz"
    Request req = CreateRequest({
        {"Authorization", "Basic dGVzdHVzZXI6d3JvbmdwYXNz"}
    });
    
    AuthResult result = auth.Authenticate(req);
    
    EXPECT_FALSE(result.authenticated);
    EXPECT_EQ(result.error, "Invalid password");
}

TEST(BasicAuthenticatorTest, UnknownUser) {
    BasicAuthenticator auth;
    auth.AddUserWithPassword("knownuser", "pass");
    
    // Base64("unknownuser:pass") = "dW5rbm93bnVzZXI6cGFzcw=="
    Request req = CreateRequest({
        {"Authorization", "Basic dW5rbm93bnVzZXI6cGFzcw=="}
    });
    
    AuthResult result = auth.Authenticate(req);
    
    EXPECT_FALSE(result.authenticated);
    EXPECT_EQ(result.error, "Unknown user");
}

TEST(BasicAuthenticatorTest, MissingAuthHeader) {
    BasicAuthenticator auth;
    auth.AddUserWithPassword("testuser", "testpass");
    
    Request req = CreateRequest({});
    AuthResult result = auth.Authenticate(req);
    
    EXPECT_FALSE(result.authenticated);
    EXPECT_EQ(result.error, "Missing Authorization header");
}

TEST(BasicAuthenticatorTest, NotBasicAuth) {
    BasicAuthenticator auth;
    auth.AddUserWithPassword("testuser", "testpass");
    
    Request req = CreateRequest({
        {"Authorization", "Bearer some-token"}
    });
    
    AuthResult result = auth.Authenticate(req);
    
    EXPECT_FALSE(result.authenticated);
    EXPECT_EQ(result.error, "Not Basic authentication");
}

TEST(BasicAuthenticatorTest, MultipleUsers) {
    BasicAuthenticator auth;
    auth.AddUserWithPassword("user1", "pass1", "tenant1");
    auth.AddUserWithPassword("user2", "pass2", "tenant2");
    
    // Test user1
    Request req1 = CreateRequest({
        {"Authorization", "Basic dXNlcjE6cGFzczE="}  // user1:pass1
    });
    AuthResult result1 = auth.Authenticate(req1);
    EXPECT_TRUE(result1.authenticated);
    EXPECT_EQ(result1.tenant_id.value(), "tenant1");
    
    // Test user2
    Request req2 = CreateRequest({
        {"Authorization", "Basic dXNlcjI6cGFzczI="}  // user2:pass2
    });
    AuthResult result2 = auth.Authenticate(req2);
    EXPECT_TRUE(result2.authenticated);
    EXPECT_EQ(result2.tenant_id.value(), "tenant2");
}

// ============================================================================
// BearerAuthenticator Tests
// ============================================================================

TEST(BearerAuthenticatorTest, ValidToken) {
    BearerAuthenticator auth;
    auth.AddToken("secret-token-123", "tenant1");
    
    Request req = CreateRequest({
        {"Authorization", "Bearer secret-token-123"}
    });
    
    AuthResult result = auth.Authenticate(req);
    
    EXPECT_TRUE(result.authenticated);
    EXPECT_TRUE(result.tenant_id.has_value());
    EXPECT_EQ(result.tenant_id.value(), "tenant1");
}

TEST(BearerAuthenticatorTest, InvalidToken) {
    BearerAuthenticator auth;
    auth.AddToken("valid-token");
    
    Request req = CreateRequest({
        {"Authorization", "Bearer invalid-token"}
    });
    
    AuthResult result = auth.Authenticate(req);
    
    EXPECT_FALSE(result.authenticated);
    EXPECT_EQ(result.error, "Invalid or revoked token");
}

TEST(BearerAuthenticatorTest, RevokedToken) {
    BearerAuthenticator auth;
    auth.AddToken("token-to-revoke");
    
    // First verify it works
    Request req = CreateRequest({
        {"Authorization", "Bearer token-to-revoke"}
    });
    EXPECT_TRUE(auth.Authenticate(req).authenticated);
    
    // Revoke it
    auth.RevokeToken("token-to-revoke");
    
    // Now it should fail
    AuthResult result = auth.Authenticate(req);
    EXPECT_FALSE(result.authenticated);
    EXPECT_EQ(result.error, "Invalid or revoked token");
}

TEST(BearerAuthenticatorTest, MissingAuthHeader) {
    BearerAuthenticator auth;
    auth.AddToken("some-token");
    
    Request req = CreateRequest({});
    AuthResult result = auth.Authenticate(req);
    
    EXPECT_FALSE(result.authenticated);
    EXPECT_EQ(result.error, "Missing Authorization header");
}

TEST(BearerAuthenticatorTest, InvalidFormat) {
    BearerAuthenticator auth;
    auth.AddToken("valid-token");
    
    Request req = CreateRequest({
        {"Authorization", "Basic dGVzdA=="}  // Not Bearer
    });
    
    AuthResult result = auth.Authenticate(req);
    
    EXPECT_FALSE(result.authenticated);
    EXPECT_EQ(result.error, "Invalid Bearer token format");
}

TEST(BearerAuthenticatorTest, MultipleTokens) {
    BearerAuthenticator auth;
    auth.AddToken("token1", "tenant1");
    auth.AddToken("token2", "tenant2");
    
    Request req1 = CreateRequest({{"Authorization", "Bearer token1"}});
    EXPECT_EQ(auth.Authenticate(req1).tenant_id.value(), "tenant1");
    
    Request req2 = CreateRequest({{"Authorization", "Bearer token2"}});
    EXPECT_EQ(auth.Authenticate(req2).tenant_id.value(), "tenant2");
}

// ============================================================================
// HeaderAuthenticator Tests
// ============================================================================

TEST(HeaderAuthenticatorTest, ValidTenant) {
    HeaderAuthenticator auth;
    auth.SetTenantHeader("X-Scope-OrgID");
    auth.AddValidTenant("tenant1");
    
    Request req = CreateRequest({
        {"X-Scope-OrgID", "tenant1"}
    });
    
    AuthResult result = auth.Authenticate(req);
    
    EXPECT_TRUE(result.authenticated);
    EXPECT_TRUE(result.tenant_id.has_value());
    EXPECT_EQ(result.tenant_id.value(), "tenant1");
}

TEST(HeaderAuthenticatorTest, InvalidTenant) {
    HeaderAuthenticator auth;
    auth.AddValidTenant("tenant1");
    
    Request req = CreateRequest({
        {"X-Scope-OrgID", "invalid-tenant"}
    });
    
    AuthResult result = auth.Authenticate(req);
    
    EXPECT_FALSE(result.authenticated);
    EXPECT_TRUE(result.error.find("Invalid tenant ID") != std::string::npos);
}

TEST(HeaderAuthenticatorTest, MissingHeader) {
    HeaderAuthenticator auth;
    auth.SetRequireHeader(true);
    
    Request req = CreateRequest({});
    AuthResult result = auth.Authenticate(req);
    
    EXPECT_FALSE(result.authenticated);
    EXPECT_TRUE(result.error.find("Missing tenant header") != std::string::npos);
}

TEST(HeaderAuthenticatorTest, HeaderNotRequired) {
    HeaderAuthenticator auth;
    auth.SetRequireHeader(false);
    
    Request req = CreateRequest({});
    AuthResult result = auth.Authenticate(req);
    
    EXPECT_TRUE(result.authenticated);
    EXPECT_FALSE(result.tenant_id.has_value());
}

TEST(HeaderAuthenticatorTest, NoValidation) {
    HeaderAuthenticator auth;
    auth.SetValidateTenants(false);
    
    Request req = CreateRequest({
        {"X-Scope-OrgID", "any-tenant"}
    });
    
    AuthResult result = auth.Authenticate(req);
    
    EXPECT_TRUE(result.authenticated);
    EXPECT_EQ(result.tenant_id.value(), "any-tenant");
}

TEST(HeaderAuthenticatorTest, CustomHeaderName) {
    HeaderAuthenticator auth;
    auth.SetTenantHeader("X-Custom-Tenant");
    auth.SetValidateTenants(false);
    
    Request req = CreateRequest({
        {"X-Custom-Tenant", "my-tenant"}
    });
    
    AuthResult result = auth.Authenticate(req);
    
    EXPECT_TRUE(result.authenticated);
    EXPECT_EQ(result.tenant_id.value(), "my-tenant");
}

// ============================================================================
// CompositeAuthenticator Tests
// ============================================================================

TEST(CompositeAuthenticatorTest, AnyMode_FirstSucceeds) {
    auto basic = std::make_shared<BasicAuthenticator>();
    basic->AddUserWithPassword("user", "pass");
    
    auto bearer = std::make_shared<BearerAuthenticator>();
    bearer->AddToken("token");
    
    CompositeAuthenticator auth;
    auth.SetMode(CompositeAuthenticator::Mode::ANY);
    auth.AddAuthenticator(basic);
    auth.AddAuthenticator(bearer);
    
    // Use Basic auth
    Request req = CreateRequest({
        {"Authorization", "Basic dXNlcjpwYXNz"}  // user:pass
    });
    
    AuthResult result = auth.Authenticate(req);
    EXPECT_TRUE(result.authenticated);
}

TEST(CompositeAuthenticatorTest, AnyMode_SecondSucceeds) {
    auto basic = std::make_shared<BasicAuthenticator>();
    basic->AddUserWithPassword("user", "pass");
    
    auto bearer = std::make_shared<BearerAuthenticator>();
    bearer->AddToken("token", "tenant1");
    
    CompositeAuthenticator auth;
    auth.SetMode(CompositeAuthenticator::Mode::ANY);
    auth.AddAuthenticator(basic);
    auth.AddAuthenticator(bearer);
    
    // Use Bearer auth
    Request req = CreateRequest({
        {"Authorization", "Bearer token"}
    });
    
    AuthResult result = auth.Authenticate(req);
    EXPECT_TRUE(result.authenticated);
    EXPECT_EQ(result.tenant_id.value(), "tenant1");
}

TEST(CompositeAuthenticatorTest, AnyMode_AllFail) {
    auto basic = std::make_shared<BasicAuthenticator>();
    basic->AddUserWithPassword("user", "pass");
    
    auto bearer = std::make_shared<BearerAuthenticator>();
    bearer->AddToken("token");
    
    CompositeAuthenticator auth;
    auth.SetMode(CompositeAuthenticator::Mode::ANY);
    auth.AddAuthenticator(basic);
    auth.AddAuthenticator(bearer);
    
    // Invalid auth
    Request req = CreateRequest({
        {"Authorization", "Bearer invalid"}
    });
    
    AuthResult result = auth.Authenticate(req);
    EXPECT_FALSE(result.authenticated);
}

TEST(CompositeAuthenticatorTest, AllMode_BothSucceed) {
    auto header = std::make_shared<HeaderAuthenticator>();
    header->SetValidateTenants(false);
    
    auto bearer = std::make_shared<BearerAuthenticator>();
    bearer->AddToken("token");
    
    CompositeAuthenticator auth;
    auth.SetMode(CompositeAuthenticator::Mode::ALL);
    auth.AddAuthenticator(header);
    auth.AddAuthenticator(bearer);
    
    Request req = CreateRequest({
        {"X-Scope-OrgID", "tenant1"},
        {"Authorization", "Bearer token"}
    });
    
    AuthResult result = auth.Authenticate(req);
    EXPECT_TRUE(result.authenticated);
    EXPECT_EQ(result.tenant_id.value(), "tenant1");
}

TEST(CompositeAuthenticatorTest, AllMode_OneFails) {
    auto header = std::make_shared<HeaderAuthenticator>();
    header->SetRequireHeader(true);
    
    auto bearer = std::make_shared<BearerAuthenticator>();
    bearer->AddToken("token");
    
    CompositeAuthenticator auth;
    auth.SetMode(CompositeAuthenticator::Mode::ALL);
    auth.AddAuthenticator(header);
    auth.AddAuthenticator(bearer);
    
    // Missing header
    Request req = CreateRequest({
        {"Authorization", "Bearer token"}
    });
    
    AuthResult result = auth.Authenticate(req);
    EXPECT_FALSE(result.authenticated);
}
