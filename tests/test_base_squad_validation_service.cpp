// Base 侧小队资格盖章与凭证签发编排单测（D11 issuer=Base 批③验收，
// 04 §4.3 分批表：资格单测——存在/缺席/冻结/序无关 hash 稳定 + 签发字段
// 口径）。假 WorldSquadClient（哑管道同接口，录帧不拨线）+ 可编程资格
// 视图，逐例注入 972/975 驱动编排裁决。
#include "base/squad_validation_service.hpp"

#include "squad_instance/roster_hash.hpp"

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using base::SquadCapacityGrant;
using base::SquadEligibilityView;
using base::SquadValidationCounters;
using base::SquadValidationRequest;
using base::SquadValidationService;
using base::SquadValidationServiceConfig;
using base::WorldSquadClient;
using base::WorldSquadClientConfig;
using base::WorldSquadLinkState;

int g_failures = 0;

#define TEST_ASSERT(cond, msg)                                                     \
    do {                                                                           \
        if (!(cond)) {                                                             \
            std::cout << "FAIL: " << (msg) << " (" << #cond << ") at "             \
                      << __LINE__ << std::endl;                                    \
            ++g_failures;                                                          \
            return false;                                                          \
        }                                                                          \
    } while (0)

// 发送录帧。
struct SentReply {
    std::uint64_t request_id = 0;
    bool accepted = false;
    std::string roster_hash;
    std::string failed_members;
    std::int32_t code = 0;
};
struct SentCapacity {
    std::uint64_t request_id = 0;
    std::uint64_t party_id = 0;
    std::uint64_t instance_id = 0;
    std::uint64_t cell_id = 0;
    std::string roster_hash;
    std::int32_t slots = 0;
};
struct SentCredential {
    std::uint64_t request_id = 0;
    std::uint64_t reservation_id = 0;
    std::uint64_t credential_id = 0;
    std::uint64_t player_id = 0;
    std::uint64_t squad_id = 0;
    std::uint64_t instance_id = 0;
    std::uint64_t cell_id = 0;
    std::string roster_hash;
    std::uint64_t issued_at = 0;
    std::uint64_t expires_at = 0;
    std::string issuer;
};

// 假哑管道：录发送帧、存回调、测试直接注入入向（同接口不拨线）。
class FakeSquadLink final : public WorldSquadClient {
public:
    std::function<void(const SquadValidationRequest&)> on_validation;
    std::function<void(const SquadCapacityGrant&)> on_grant;
    bool up = true;

    std::vector<SentReply> replies;
    std::vector<SentCapacity> capacities;
    std::vector<SentCredential> credentials;

    bool start() override { return true; }
    void stop() override {}
    void pump(std::uint64_t) override {}
    bool linkUp() const override { return up; }
    WorldSquadLinkState state() const override { return {}; }

    void set_validation_handler(
        std::function<void(const SquadValidationRequest&)> fn) override {
        on_validation = std::move(fn);
    }
    void set_grant_handler(
        std::function<void(const SquadCapacityGrant&)> fn) override {
        on_grant = std::move(fn);
    }

    bool send_validation_reply(std::uint64_t request_id, bool accepted,
                               const std::string& roster_hash,
                               const std::string& failed_members,
                               std::int32_t code) override {
        if (!up) {
            return false;
        }
        replies.push_back(
            SentReply{request_id, accepted, roster_hash, failed_members, code});
        return true;
    }
    bool send_capacity_request(std::uint64_t request_id, std::uint64_t party_id,
                               std::uint64_t instance_id,
                               std::uint64_t cell_id,
                               const std::string& roster_hash,
                               std::int32_t slots) override {
        if (!up) {
            return false;
        }
        capacities.push_back(SentCapacity{request_id, party_id, instance_id,
                                          cell_id, roster_hash, slots});
        return true;
    }
    bool send_credential_bundle(
        std::uint64_t request_id, std::uint64_t reservation_id,
        std::uint64_t credential_id, std::uint64_t player_id,
        std::uint64_t squad_id, std::uint64_t instance_id,
        std::uint64_t cell_id, const std::string& roster_hash,
        std::uint64_t issued_at, std::uint64_t expires_at,
        const std::string& issuer) override {
        if (!up) {
            return false;
        }
        credentials.push_back(SentCredential{request_id, reservation_id,
                                             credential_id, player_id, squad_id,
                                             instance_id, cell_id, roster_hash,
                                             issued_at, expires_at, issuer});
        return true;
    }

    void inject_validation(std::uint64_t request_id, std::uint64_t party_id,
                           std::uint64_t instance_id, std::uint64_t cell_id,
                           const std::string& members) {
        SquadValidationRequest request;
        request.request_id = request_id;
        request.party_id = party_id;
        request.instance_id = instance_id;
        request.cell_id = cell_id;
        request.members = members;
        on_validation(request);
    }
    void inject_grant(std::uint64_t request_id, bool granted,
                      std::uint64_t reservation_id, std::uint64_t expires_at,
                      std::int32_t code) {
        SquadCapacityGrant grant;
        grant.request_id = request_id;
        grant.granted = granted;
        grant.reservation_id = reservation_id;
        grant.expires_at = expires_at;
        grant.code = code;
        on_grant(grant);
    }
};

// 可编程资格视图：测试事实源（在册/冻结集逐例注入）。
class FakeEligibility final : public SquadEligibilityView {
public:
    std::vector<std::uint64_t> known;
    std::vector<std::uint64_t> frozen;

    bool member_exists(std::uint64_t player_id) const override {
        for (const auto id : known) {
            if (id == player_id) {
                return true;
            }
        }
        return false;
    }
    bool member_frozen(std::uint64_t player_id) const override {
        for (const auto id : frozen) {
            if (id == player_id) {
                return true;
            }
        }
        return false;
    }
};

// 装配帮助：固定钟（T=1000），issuer=base:77。
struct Rig {
    FakeSquadLink link;
    FakeEligibility view;
    std::uint64_t now = 1000;
    std::unique_ptr<SquadValidationService> service;

    Rig() {
        SquadValidationServiceConfig config;
        config.base_id = 77;
        service = std::make_unique<SquadValidationService>(
            link, view, config, [this]() { return now; });
    }
};

// 全员合格正路径：973 盖章（序无关 hash 同 roster_hash_of）+ 974 申请
// （新关联键、槽数=成员数、字段镜像）。
bool test_eligible_roster_stamps_and_requests() {
    Rig rig;
    rig.view.known = {100, 200, 300};
    rig.link.inject_validation(501, 42, 7, 9, "300,200,100");

    TEST_ASSERT(rig.link.replies.size() == 1, "恰一帧 973");
    const auto& reply = rig.link.replies[0];
    TEST_ASSERT(reply.request_id == 501, "973 回带 972 关联键");
    TEST_ASSERT(reply.accepted, "全员合格 accepted=1");
    TEST_ASSERT(reply.code == 0, "code 0");
    TEST_ASSERT(reply.failed_members.empty(), "合格无拒因清单");
    TEST_ASSERT(reply.roster_hash ==
                    squad_instance::roster_hash_of({100, 200, 300}),
                "盖章与 cell 侧同算法同值");

    TEST_ASSERT(rig.link.capacities.size() == 1, "恰一帧 974");
    const auto& cap = rig.link.capacities[0];
    TEST_ASSERT(cap.request_id == 1, "974 签发方单调关联键自 1");
    TEST_ASSERT(cap.party_id == 42 && cap.instance_id == 7 && cap.cell_id == 9,
                "974 字段镜像 972");
    TEST_ASSERT(cap.roster_hash == reply.roster_hash, "974 回带盖章");
    TEST_ASSERT(cap.slots == 3, "槽数=成员数");

    TEST_ASSERT(rig.service->pending_capacity() == 1, "申请在册待裁决");
    const auto counters = rig.service->counters();
    TEST_ASSERT(counters.validations_in == 1 && counters.accepted_out == 1 &&
                    counters.capacity_requests_out == 1,
                "计数三面");
    return true;
}

// 序无关 hash 稳定：同成员不同入队次序 → 同盖章（04 §4.3 盖戳口径）。
bool test_roster_hash_order_independent() {
    Rig rig_a;
    rig_a.view.known = {100, 200, 300};
    rig_a.link.inject_validation(1, 42, 7, 9, "100,200,300");
    Rig rig_b;
    rig_b.view.known = {300, 100, 200};
    rig_b.link.inject_validation(2, 42, 7, 9, "300,100,200");

    TEST_ASSERT(rig_a.link.replies.size() == 1 && rig_b.link.replies.size() == 1,
                "两路各一帧 973");
    TEST_ASSERT(rig_a.link.replies[0].roster_hash ==
                    rig_b.link.replies[0].roster_hash,
                "不同次序同盖章");
    TEST_ASSERT(!rig_a.link.replies[0].roster_hash.empty(), "盖章非空");
    return true;
}

// 缺档（code 1）：拒因成员清单回带、不盖章、不发 974。
bool test_missing_member_rejects_code1() {
    Rig rig;
    rig.view.known = {100, 200};
    rig.link.inject_validation(1, 42, 7, 9, "100,999,200");

    TEST_ASSERT(rig.link.replies.size() == 1, "恰一帧 973");
    const auto& reply = rig.link.replies[0];
    TEST_ASSERT(!reply.accepted && reply.code == 1, "缺档 code 1");
    TEST_ASSERT(reply.failed_members == "999", "拒因成员回带");
    TEST_ASSERT(reply.roster_hash.empty(), "拒绝不盖章");
    TEST_ASSERT(rig.link.capacities.empty() && rig.service->pending_capacity() == 0,
                "拒绝零申请");
    TEST_ASSERT(rig.service->counters().rejections_out == 1, "拒绝计数");
    return true;
}

// 冻结（code 2）：迁移冻结标记在册即拒。
bool test_frozen_member_rejects_code2() {
    Rig rig;
    rig.view.known = {100, 200};
    rig.view.frozen = {200};
    rig.link.inject_validation(1, 42, 7, 9, "100,200");

    TEST_ASSERT(rig.link.replies.size() == 1, "恰一帧 973");
    const auto& reply = rig.link.replies[0];
    TEST_ASSERT(!reply.accepted && reply.code == 2, "冻结 code 2");
    TEST_ASSERT(reply.failed_members == "200", "拒因成员回带");
    TEST_ASSERT(reply.roster_hash.empty() && rig.link.capacities.empty(),
                "拒绝不盖章零申请");
    return true;
}

// 缺档与冻结并存：报缺档码（1），清单带命中码类别成员。
bool test_mixed_missing_wins_code() {
    Rig rig;
    rig.view.known = {100};
    rig.view.frozen = {100};
    rig.link.inject_validation(1, 42, 7, 9, "100,999");

    TEST_ASSERT(rig.link.replies.size() == 1, "恰一帧 973");
    const auto& reply = rig.link.replies[0];
    TEST_ASSERT(!reply.accepted && reply.code == 1, "并存报缺档码");
    TEST_ASSERT(reply.failed_members == "999", "清单带缺档成员");
    return true;
}

// 名册空/非法（code 3）：空串与坏段（空位/非数字）皆拒。
bool test_bad_roster_rejects_code3() {
    Rig rig;
    rig.link.inject_validation(1, 42, 7, 9, "");
    TEST_ASSERT(rig.link.replies.size() == 1 && !rig.link.replies[0].accepted &&
                    rig.link.replies[0].code == 3,
                "空名册 code 3");

    rig.link.inject_validation(2, 42, 7, 9, "100,,200");
    TEST_ASSERT(rig.link.replies.size() == 2 && !rig.link.replies[1].accepted &&
                    rig.link.replies[1].code == 3,
                "空段 code 3");

    rig.link.inject_validation(3, 42, 7, 9, "100,x");
    TEST_ASSERT(rig.link.replies.size() == 3 && !rig.link.replies[2].accepted &&
                    rig.link.replies[2].code == 3,
                "非数字 code 3");

    TEST_ASSERT(rig.service->counters().bad_rosters == 3, "非法名册计数");
    TEST_ASSERT(rig.link.capacities.empty(), "非法名册零申请");
    return true;
}

// 签发字段口径：975 granted → 逐成员 976（同 974 关联键、预留 id 回带、
// 凭证号单调唯一、字段镜像、窗原值、issuer=base:<id>）。
bool test_grant_issues_bundle() {
    Rig rig;
    rig.view.known = {100, 200, 300};
    rig.link.inject_validation(501, 42, 7, 9, "100,200,300");
    TEST_ASSERT(rig.link.capacities.size() == 1, "前置 974 已发");

    rig.link.inject_grant(1, true, 9, 2000, 0);  // 974 关联键 1，窗至 2000

    TEST_ASSERT(rig.link.credentials.size() == 3, "逐成员三帧 976");
    std::uint64_t expect_credential = 1;
    const std::vector<std::uint64_t> expect_players = {100, 200, 300};
    for (std::size_t i = 0; i < rig.link.credentials.size(); ++i) {
        const auto& frame = rig.link.credentials[i];
        TEST_ASSERT(frame.request_id == 1, "976 与 974 同号关联");
        TEST_ASSERT(frame.reservation_id == 9, "预留 id 回带");
        TEST_ASSERT(frame.credential_id == expect_credential,
                    "凭证号单调自 1");
        TEST_ASSERT(frame.player_id == expect_players[i], "成员绑定");
        TEST_ASSERT(frame.squad_id == 42 && frame.instance_id == 7 &&
                        frame.cell_id == 9,
                    "目标镜像");
        TEST_ASSERT(frame.roster_hash == rig.link.replies[0].roster_hash,
                    "盖章透传");
        TEST_ASSERT(frame.issued_at == 1000, "签发时刻=钟");
        TEST_ASSERT(frame.expires_at == 2000, "有效期=预留窗原值");
        TEST_ASSERT(frame.issuer == "base:77", "issuer=base:<id>");
        ++expect_credential;
    }

    const auto counters = rig.service->counters();
    TEST_ASSERT(counters.credentials_out == 3 && counters.grants_in == 1,
                "签发计数");
    TEST_ASSERT(rig.service->pending_capacity() == 0, "裁决后收册");
    return true;
}

// 签发时预留窗已过：不放凭证（04 §4.3 预留生命周期）。
bool test_expired_grant_issues_nothing() {
    Rig rig;
    rig.view.known = {100, 200};
    rig.link.inject_validation(1, 42, 7, 9, "100,200");
    TEST_ASSERT(rig.link.capacities.size() == 1, "前置 974 已发");

    rig.now = 3000;  // 窗至 2000，签发钟已过窗
    rig.link.inject_grant(1, true, 9, 2000, 0);

    TEST_ASSERT(rig.link.credentials.empty(), "窗已过零凭证");
    TEST_ASSERT(rig.service->counters().grants_expired == 1, "过期计数");
    TEST_ASSERT(rig.service->pending_capacity() == 0, "收册");
    return true;
}

// 容量拒绝（granted=0）：整批不放凭证，收册。
bool test_rejected_grant_issues_nothing() {
    Rig rig;
    rig.view.known = {100, 200};
    rig.link.inject_validation(1, 42, 7, 9, "100,200");
    rig.link.inject_grant(1, false, 0, 0, 1);  // code 1 无容量

    TEST_ASSERT(rig.link.credentials.empty(), "拒绝零凭证");
    TEST_ASSERT(rig.service->counters().grants_rejected == 1, "拒绝计数");
    TEST_ASSERT(rig.service->pending_capacity() == 0, "收册");
    return true;
}

// 重放/未知号 grant 不签发；裁决一次性（成功后重放同号不再发）。
bool test_grant_unknown_or_replayed_ignored() {
    Rig rig;
    rig.view.known = {100};
    rig.link.inject_validation(1, 42, 7, 9, "100");
    rig.link.inject_grant(1, true, 9, 5000, 0);
    TEST_ASSERT(rig.link.credentials.size() == 1, "首次签发一帧");

    rig.link.inject_grant(1, true, 9, 5000, 0);   // 同号重放
    rig.link.inject_grant(99, true, 8, 5000, 0);  // 未知号
    TEST_ASSERT(rig.link.credentials.size() == 1, "重放/未知号零新增");
    TEST_ASSERT(rig.service->counters().grants_unknown == 2, "未知号计数");
    return true;
}

// 链路未 up：发送面拒发只计数不崩溃（哑管道不重试）。
bool test_link_down_counts_send_failures() {
    Rig rig;
    rig.link.up = false;
    rig.view.known = {100, 200};
    rig.link.inject_validation(1, 42, 7, 9, "100,200");

    const auto counters = rig.service->counters();
    TEST_ASSERT(counters.send_failures >= 1, "拒发计数前进");
    TEST_ASSERT(rig.link.replies.empty() && rig.link.capacities.empty(),
                "未 up 零落地帧");
    TEST_ASSERT(rig.service->pending_capacity() == 0, "未发即无在册");
    return true;
}

}  // namespace

int main() {
    struct Named {
        const char* name;
        bool (*fn)();
    };
    const Named tests[] = {
        {"eligible_roster_stamps_and_requests", test_eligible_roster_stamps_and_requests},
        {"roster_hash_order_independent", test_roster_hash_order_independent},
        {"missing_member_rejects_code1", test_missing_member_rejects_code1},
        {"frozen_member_rejects_code2", test_frozen_member_rejects_code2},
        {"mixed_missing_wins_code", test_mixed_missing_wins_code},
        {"bad_roster_rejects_code3", test_bad_roster_rejects_code3},
        {"grant_issues_bundle", test_grant_issues_bundle},
        {"expired_grant_issues_nothing", test_expired_grant_issues_nothing},
        {"rejected_grant_issues_nothing", test_rejected_grant_issues_nothing},
        {"grant_unknown_or_replayed_ignored", test_grant_unknown_or_replayed_ignored},
        {"link_down_counts_send_failures", test_link_down_counts_send_failures},
    };

    for (const auto& t : tests) {
        std::cout << "Running test_" << t.name << "... " << std::flush;
        if (t.fn()) {
            std::cout << "PASSED" << std::endl;
        } else {
            std::cout << "FAILED" << std::endl;
            std::cerr << "base_squad_validation_service_tests failed" << std::endl;
            return 1;
        }
    }

    std::cout << "base_squad_validation_service_tests passed" << std::endl;
    return 0;
}
