#include "common/file.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common/unique_fd.h"

namespace tvmm {

Result<std::vector<std::byte>> ReadFile(const std::string& path) {
  UniqueFd fd(::open(path.c_str(), O_RDONLY | O_CLOEXEC));
  if (!fd.valid()) return FailErrno("open " + path);

  struct stat st {};
  if (::fstat(fd.get(), &st) != 0) return FailErrno("fstat " + path);

  std::vector<std::byte> data(static_cast<std::size_t>(st.st_size));
  std::size_t done = 0;
  while (done < data.size()) {
    const ssize_t n = ::read(fd.get(), data.data() + done, data.size() - done);
    if (n < 0) {
      if (errno == EINTR) continue;
      return FailErrno("read " + path);
    }
    if (n == 0) break;
    done += static_cast<std::size_t>(n);
  }
  data.resize(done);
  return data;
}

}  // namespace tvmm
