#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/capability.h>
#include <seccomp.h>
#include <fcntl.h>

#include "config.h"

int pivot_root(const char *new_root, const char *put_old)
{
	return syscall(SYS_pivot_root, new_root, put_old);
}

int apply_seccomp()
{
	scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_KILL);
	if (!ctx) return -1;

	/* Whitelist */
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(read), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit_group), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(execve), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(brk), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mmap), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(munmap), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fstat), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(stat), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(openat), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(close), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigaction), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ioctl), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(futex), 0);
	seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mprotect), 0);

	return seccomp_load(ctx);
}

/*
 * Drop all capabilities so root inside isn't root outside
 */
int drop_caps()
{
	cap_t caps = cap_get_proc();
	if (cap_clear(caps) == -1) return -1;
	if (cap_set_proc(caps) == -1) return -1;
	cap_free(caps);
	return 0;
}

int container_main(void *arg)
{
	/* Wait for parent to set up UID/GID maps and network */
	while (getuid() != 0) usleep(100);

	/* OverlayFS */
	char opts[512];
	snprintf(opts, sizeof(opts), "lowerdir=%s,upperdir=%s,workdir=%s",
		  LOWER_DIR, UPPER_DIR, WORK_DIR);
	if (mount("overlay", MERGED_DIR, "overlay", 0, opts) < 0) {
		perror("mount-overlayfs");
		return 1;
	}

	if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0) {
		perror("mount-private");
		return 1;
	}

	mount(MERGED_DIR, MERGED_DIR, NULL, MS_BIND | MS_REC, NULL);
	char dev_path[512];
    snprintf(dev_path, sizeof(dev_path), "%s/dev", MERGED_DIR);
    mount("/dev", dev_path, NULL, MS_BIND | MS_REC, NULL);

	sethostname(hostname, strlen(hostname));
	char old_root[512];
	snprintf(old_root, sizeof(old_root), "%s/old_root", MERGED_DIR);
	mkdir(old_root, 0700);

	if (pivot_root(MERGED_DIR, old_root) < 0) {
		perror("pivot_root");
		return 1;
	}
	chdir("/");
	umount2("/old_root", MNT_DETACH);
	rmdir("/old_root");

	/* Mount these so have basic shell operations and networking */
	mount("proc", "/proc", "proc", 0, NULL);
	mount("tmpfs", "/tmp", "tmpfs", 0, NULL);
	mount("sysfs", "/sys", "sysfs", 0, NULL);
	
	/* Lockdown */
	if (drop_caps() < 0) { perror("cap_set_proc"); return 1; }
	if (apply_seccomp() < 0) { perror("seccomp_load"); return 1; }

	system("ip link set lo up");
	system("ip link set veth-guest up");
	system("ip addr add 192.168.10.2/24 dev veth-guest");
	system("ip route add default via 192.168.10.1");

	char *args[] = { "/bin/sh", NULL };
	execvp(args[0], args);

	return 0;
}

void apply_limits(pid_t pid)
{
	char path[512], buf[512];
	snprintf(path, sizeof(path), "/sys/fs/cgroup/box_%d", pid);
	mkdir(path, 0755);

	snprintf(buf, sizeof(buf), "%s/cgroup.procs", path);
	int fd = open(buf, O_WRONLY);
	dprintf(fd, "%d", pid);
	close(fd);

	snprintf(buf, sizeof(buf), "%s/memory.max", path);
	fd = open(buf, O_WRONLY);
	dprintf(fd, "%s", mem_max);
	close(fd);

	/* Prevent fork bombs */
    snprintf(buf, sizeof(buf), "%s/pids.max", path);
    fd = open(buf, O_WRONLY);
    if (fd >= 0) { dprintf(fd, "64"); close(fd); }
}

int main()
{
	char *stack = malloc(STACK_SIZE);

	if (!stack) {
		perror("malloc");
		return 1;
	}

	/* Prepare Overlay Dirs */
	mkdir(OVERLAY_DATA, 0755);
	mkdir(UPPER_DIR, 0755);
	mkdir(WORK_DIR, 0755);
	mkdir(MERGED_DIR, 0755);

	int flags = CLONE_NEWNS | CLONE_NEWCGROUP | CLONE_NEWPID | 
	            CLONE_NEWIPC | CLONE_NEWNET | CLONE_NEWUTS | 
	            CLONE_NEWUSER | SIGCHLD;

	pid_t pid = clone(container_main, stack + STACK_SIZE, flags, NULL);

	if (pid < 0) {
		perror("clone");
		return 1;
	}

	/* Set up UID/GID mapping so host user = container root */
	char map_path[256], buf[64];
	snprintf(buf, sizeof(buf), "0 %d 1", getuid());
	
	snprintf(map_path, sizeof(map_path), "/proc/%d/uid_map", pid);
	int fd = open(map_path, O_WRONLY);
	write(fd, buf, strlen(buf));
	close(fd);

	snprintf(map_path, sizeof(map_path), "/proc/%d/setgroups", pid);
	fd = open(map_path, O_WRONLY);
	write(fd, "deny", 4);
	close(fd);

	snprintf(map_path, sizeof(map_path), "/proc/%d/gid_map", pid);
	fd = open(map_path, O_WRONLY);
	write(fd, buf, sizeof(buf));
	close(fd);

	apply_limits(pid);

	char net_cmd[512];
	snprintf(net_cmd, sizeof(net_cmd), "./setup_net.sh %d", pid);
	system(net_cmd);

	waitpid(pid, NULL, 0);

	umount2(MERGED_DIR, MNT_DETACH);
	system("rm -rf " UPPER_DIR "/* " WORK_DIR "/*");

	char rmdir_cmd[512];
	snprintf(rmdir_cmd, sizeof(rmdir_cmd), "rmdir /sys/fs/cgroup/box_%d", pid);
	system(rmdir_cmd);

	printf("Box %d exited and cleaned up.\n", pid);
	return 0;
}
