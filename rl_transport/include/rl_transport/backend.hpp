#pragma once

namespace rl {

// 传输后端。M1 只有 UDS 可用，TCP 和共享内存会构造成功，但一收发就抛异常。
enum class Backend {
  kUds,
  kTcp,
  kShm,
};

}  // namespace rl
