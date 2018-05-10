#ifndef HMR_UTILS_H
#define HMR_UTILS_H
enum ibv_wr_opcode hmr_get_opcode_from_msg(struct hmr_msg *msg);
enum ibv_wr_opcode hmr_get_opcode_from_task(struct hmr_task *task);
#endif
