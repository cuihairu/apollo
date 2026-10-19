-- contract_handlers.lua — 契约路由 handler 面（占位模块，玩法批填充）。
--
-- 为什么是全局函数：load_contract 的路由完整性闸走 resolve_handler——裸名
-- （无点）解析为 Lua 全局，点路径仅支持一层 module.func（scripting-lua
-- §3.4）。契约 routes 的 handler 名是消息名（apollo_gen 吐），故逐条落成
-- 全局。装载闸只验存在性，不验语义。
--
-- 同步纪律：apollo_gen 新增 reflect 消息即新 route，本文件必须同步补行——
-- tests/test_contract_handlers.cpp 读 golden 契约 + 本文件全装载路径跑闸，
-- 缺行即红。玩法批按消息语义填实体（当前全为无操作占位）。
-- 迁移面：全局名是装载闸的裸名解析面；模块表化（模块私有状态进模块表、
-- 不进 _G，§3.1）随 §3.1 模块装载器批。
session_up = function() end
session_down = function() end
session_moved = function() end
session_kicked = function() end
directory_snapshot_request = function() end
directory_snapshot_reply = function() end
gateway_client_disconnect = function() end
session_resume_request = function() end
session_resume_reply = function() end
gateway_assign_request = function() end
gateway_assign_response = function() end
player_activate_request = function() end
player_activate_response = function() end
player_bind_session_request = function() end
player_bind_session_response = function() end
player_assign_world_request = function() end
player_assign_world_response = function() end
player_resolve_route_request = function() end
player_resolve_route_response = function() end
instance_spawn_request = function() end
instance_spawn_reply = function() end
battle_input = function() end
battle_state_sync = function() end
battle_result = function() end
cell_create_entity = function() end
cell_destroy_entity = function() end
cell_entity_move = function() end
cell_cross_border = function() end
cell_skill_cast = function() end
cell_intent_ack = function() end
logger_line_push = function() end
metrics_snapshot = function() end
internal_envelope = function() end
player_freeze_request = function() end
player_freeze_reply = function() end
player_snapshot_request = function() end
player_snapshot_reply = function() end
player_handoff_request = function() end
player_handoff_reply = function() end
rpc_request = function() end
rpc_response = function() end
player_migrate_request = function() end
player_migrate_reply = function() end
debug_eval_request = function() end
debug_eval_result = function() end
client_deliver = function() end
party_create_intent = function() end
party_join_intent = function() end
squad_launch_intent = function() end
squad_enter_intent = function() end
squad_start_intent = function() end
squad_kill_intent = function() end
module_rollout_request = function() end
module_rollout_reply = function() end
gateway_drain_request = function() end
gateway_drain_reply = function() end
world_drain_request = function() end
world_drain_reply = function() end
player_score_credit_request = function() end
player_score_credit_reply = function() end
cell_register_request = function() end
cell_register_reply = function() end
