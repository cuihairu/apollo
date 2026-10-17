// RemoteEntityCall 四件套测试（RemoteEntityRef / RemoteMethodSchema /
// RouteResolver / RpcTrace）。
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "apollo/net/remote/entity_ref.hpp"
#include "apollo/net/remote/method_schema.hpp"
#include "apollo/net/remote/route_resolver.hpp"
#include "apollo/net/remote/rpc_trace.hpp"

using namespace apollo::net::remote;
using namespace apollo::net::trace;

// ── RemoteEntityRef ──────────────────────────────────────────────

TEST(RemoteEntityRef, DefaultConstruction) {
    RemoteEntityRef ref;
    EXPECT_EQ(ref.entity_id, 0u);
    EXPECT_EQ(ref.domain, EntityDomain::Anchor);
    EXPECT_EQ(ref.target_app, "");
    EXPECT_EQ(ref.role, AuthorityRole::Authoritative);
    EXPECT_EQ(ref.route_version, 0u);
    EXPECT_EQ(ref.shard_key, 0u);
}

TEST(RemoteEntityRef, FieldAccess) {
    RemoteEntityRef ref;
    ref.entity_id     = 42;
    ref.domain        = EntityDomain::World;
    ref.target_app    = "world-1";
    ref.role          = AuthorityRole::Replica;
    ref.route_version = 7;
    ref.shard_key     = 3;

    EXPECT_EQ(ref.entity_id, 42u);
    EXPECT_EQ(ref.domain, EntityDomain::World);
    EXPECT_EQ(ref.target_app, "world-1");
    EXPECT_EQ(ref.role, AuthorityRole::Replica);
    EXPECT_EQ(ref.route_version, 7u);
    EXPECT_EQ(ref.shard_key, 3u);
}

// ── RemoteMethodSchema ───────────────────────────────────────────

TEST(RemoteMethodSchema, DefaultConstruction) {
    RemoteMethodSchema schema;
    EXPECT_EQ(schema.method_name, "");
    EXPECT_EQ(schema.method_alias, "");
    EXPECT_EQ(schema.mode, InvokeMode::OneWay);
    EXPECT_EQ(schema.target_domain, EntityDomain::Anchor);
    EXPECT_TRUE(schema.arg_types.empty());
    EXPECT_EQ(schema.timeout, std::chrono::milliseconds(5000));
    EXPECT_FALSE(schema.idempotent);
}

TEST(RemoteMethodSchema, FieldAccess) {
    RemoteMethodSchema schema;
    schema.method_name   = "PlayerAnchor.send";
    schema.method_alias  = "PAS";
    schema.mode          = InvokeMode::RequestReply;
    schema.target_domain = EntityDomain::Anchor;
    schema.arg_types     = {"int64", "bytes"};
    schema.timeout       = std::chrono::milliseconds(2000);
    schema.idempotent    = true;

    EXPECT_EQ(schema.method_name, "PlayerAnchor.send");
    EXPECT_EQ(schema.method_alias, "PAS");
    EXPECT_EQ(schema.mode, InvokeMode::RequestReply);
    EXPECT_EQ(schema.target_domain, EntityDomain::Anchor);
    EXPECT_EQ(schema.arg_types.size(), 2u);
    EXPECT_EQ(schema.arg_types[0], "int64");
    EXPECT_EQ(schema.arg_types[1], "bytes");
    EXPECT_EQ(schema.timeout, std::chrono::milliseconds(2000));
    EXPECT_TRUE(schema.idempotent);
}

// ── RouteResolver ────────────────────────────────────────────────

TEST(RouteResolver, EmptyResolve) {
    RouteResolver resolver;
    auto result = resolver.resolve(42);
    EXPECT_FALSE(result.has_value());
}

TEST(RouteResolver, UpdateAndResolve) {
    RouteResolver resolver;
    RemoteEntityRef ref;
    ref.entity_id     = 42;
    ref.target_app    = "world-1";
    ref.route_version = 1;
    resolver.update_route(ref);

    auto result = resolver.resolve(42);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->entity_id, 42u);
    EXPECT_EQ(result->target_app, "world-1");
    EXPECT_EQ(resolver.size(), 1u);
}

TEST(RouteResolver, VersionBumpOnUpdate) {
    RouteResolver resolver;
    RemoteEntityRef ref;
    ref.entity_id = 42;
    ref.target_app = "world-1";
    resolver.update_route(ref);
    EXPECT_EQ(resolver.route_version(), 1u);

    RemoteEntityRef ref2;
    ref2.entity_id = 43;
    ref2.target_app = "world-2";
    resolver.update_route(ref2);
    EXPECT_EQ(resolver.route_version(), 2u);
}

TEST(RouteResolver, OverwriteSameEntity) {
    RouteResolver resolver;
    RemoteEntityRef ref;
    ref.entity_id = 42;
    ref.target_app = "world-1";
    resolver.update_route(ref);

    RemoteEntityRef ref2;
    ref2.entity_id = 42;
    ref2.target_app = "world-2";
    resolver.update_route(ref2);

    auto result = resolver.resolve(42);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->target_app, "world-2");
    EXPECT_EQ(resolver.size(), 1u);
}

TEST(RouteResolver, Clear) {
    RouteResolver resolver;
    RemoteEntityRef ref;
    ref.entity_id = 42;
    resolver.update_route(ref);
    EXPECT_EQ(resolver.size(), 1u);

    resolver.clear();
    EXPECT_EQ(resolver.size(), 0u);
    EXPECT_FALSE(resolver.resolve(42).has_value());
}

// ── RpcTrace ─────────────────────────────────────────────────────

TEST(RpcTrace, CreateContextGeneratesTraceId) {
    auto ctx = RpcTrace::create_context();
    EXPECT_TRUE(ctx.has_trace());
    EXPECT_TRUE(ctx.sampled);
}

TEST(RpcTrace, CreateContextUnique) {
    auto ctx1 = RpcTrace::create_context();
    auto ctx2 = RpcTrace::create_context();
    // trace_id should differ (random generation)
    EXPECT_NE(ctx1.trace_id, ctx2.trace_id);
}

TEST(RpcTrace, ExtractContextPreservesTraceId) {
    auto ctx = RpcTrace::create_context();
    auto extracted = RpcTrace::extract_context(ctx);
    EXPECT_EQ(extracted.trace_id, ctx.trace_id);
    EXPECT_TRUE(extracted.has_trace());
}

TEST(RpcTrace, ExtractContextPreservesSampled) {
    auto ctx = RpcTrace::create_context();
    auto extracted = RpcTrace::extract_context(ctx);
    EXPECT_EQ(extracted.sampled, ctx.sampled);
}

TEST(RpcTrace, EmitSpanWhenSampled) {
    // Inject a span sink to capture the emitted span
    SpanRecord captured;
    bool emitted = false;
    auto sink = [&](const SpanRecord& record) {
        captured = record;
        emitted = true;
    };
    set_span_sink(sink);

    auto ctx = RpcTrace::create_context();
    RemoteEntityRef ref;
    ref.entity_id = 42;
    ref.target_app = "world-1";

    RpcTrace::emit_rpc_span("PAS", ref, ctx, 100, 150);

    EXPECT_TRUE(emitted);
    EXPECT_EQ(captured.name, "PAS");
    EXPECT_EQ(captured.ctx.trace_id, ctx.trace_id);
    EXPECT_EQ(captured.start_ms, 100u);
    EXPECT_EQ(captured.end_ms, 150u);

    set_span_sink(nullptr);
}
