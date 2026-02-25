#define STACK_SIZE (1024 * 1024)
#define OVERLAY_DATA "overlay_data"
#define LOWER_DIR    "rootfs"
#define UPPER_DIR    "overlay_data/upper"
#define WORK_DIR     "overlay_data/work"
#define MERGED_DIR   "overlay_data/merged"

static const char *hostname = "sand";
static const char *mem_max  = "52428800"; // 50MB
