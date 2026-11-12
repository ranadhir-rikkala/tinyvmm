#include <fcntl.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

static const char kMarker[] = "TINYVMM_INIT_OK\n";

int main(void) {
  mkdir("/dev", 0755);
  mknod("/dev/console", S_IFCHR | 0600, makedev(5, 1));
  int fd = open("/dev/console", O_WRONLY);
  if (fd < 0) fd = STDOUT_FILENO;
  if (write(fd, kMarker, sizeof(kMarker) - 1) < 0) return 1;
  reboot(RB_AUTOBOOT);
  for (;;) pause();
}
