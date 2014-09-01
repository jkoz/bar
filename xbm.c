#include <xcb/xcb_image.h>
/* Max number of icons to have saved in memory at once */
#define ICON_CACHE_SIZE 16
/* Max filename saved to identify in cache */
#define MAX_ICON_ID_LEN 256
/* Max buffer size when reading file */
#define MAX_ICON_LINE_LEN 256
/* Max size of bytes for icon file */
#define MAX_ICON_SIZE 1024

/* TODO: replace with xcb_image_t */
typedef struct xbm_icon_t {
    int width, height, size;
    char *id;
    unsigned char *data;
} xbm_icon_t;

typedef struct xbm_icon_cache_t {
    int pos;
    int count;
    struct xbm_icon_t *icon[ICON_CACHE_SIZE];
} xbm_icon_cache_t;

static xbm_icon_cache_t xiconcache;

void
init_xbm (void)
{
    memset(&xiconcache, 0, sizeof(xbm_icon_cache_t));
}

void
free_xbm_icon_cache (const int n)
{
    if (xiconcache.icon[n] == NULL)
        return;

    if (xiconcache.icon[n]->id != NULL)
        free(xiconcache.icon[n]->id);
    if (xiconcache.icon[n]->data != NULL)
        free(xiconcache.icon[n]->data);
    free(xiconcache.icon[n]);
}

void
cleanup_xbm (void)
{
    for (int i=0;i<xiconcache.count;i++)
        free_xbm_icon_cache(i);
}

bool
read_xbm_data (FILE *f, const int n, char* info, unsigned char *data)
{
    char *lp = NULL;
    uint32_t value;
    /* Get the data type */
    if (!(lp = strrchr(info, '_')))
        lp = info;
    /* It is possible that the type will be short instead. */
    if (strncmp(lp, "_bits[]", 7) == 0) {
        /* Assume hex ' 0x4C, 0x2E, 0x43,\n 0x3A } EOF'*/
        char buf[16] = {0};
        for (int i=0;i<n;i++) {
            /* strip chars look for leading 0 */
            do {
                buf[0] = fgetc(f);
            } while (buf[0] != '0' && buf[0] != EOF);
            /* Look for hex number, leading 0 removed */
            if (fscanf(f, "x%15[0-9A-Fa-f]", buf) != 1)
                return false;
            if (sscanf(buf, "%X", &value) != 1 || value > 255)
                return false;
            data[i] = value;
        }
        /* Data is set, no errors */
        return true;
    }
    return false;
}

xbm_icon_t *
create_xbm_icon (const int width, const int height, char *filename)
{
    /* + !!(width%8) might not be needed */
    const int data_size = (width/8 + !!(width%8))*height;
    if (width < 1 || height <1 || filename == NULL || data_size > MAX_ICON_SIZE-1)
        return NULL;
    /* Alloc xbm icon, set id, width, height, size but leave data blank */
    xbm_icon_t *it = NULL;
    it = calloc(1, sizeof(xbm_icon_t));
    if (!it) {
        fprintf(stderr, "Failed to allocate new icon\n");
        return NULL;
    }
    /* Not tested yet, roll back */
    if (xiconcache.pos == ICON_CACHE_SIZE)
        xiconcache.pos = 0;
    /* Free old cache item or increment cache count */
    if (xiconcache.count == ICON_CACHE_SIZE)
        free_xbm_icon_cache(xiconcache.pos);
    else
        xiconcache.count++;
    /* Add icon to the cache so it will be freed */
    xiconcache.icon[xiconcache.pos++] = it;
    /* Find min filename len and alloc id and data */
    int filename_len = strlen(filename);
    if (filename_len > MAX_ICON_ID_LEN)
        filename_len = MAX_ICON_ID_LEN;
    it->id = calloc(filename_len, sizeof(char));
    if (!it->id) {
        fprintf(stderr, "Failed to allocate new icon id\n");
        return NULL;
    }
    it->data = calloc(data_size, sizeof(unsigned char));
    if (!it->data) {
        fprintf(stderr, "Failed to allocate new icon data\n");
        return NULL;
    }
    it->width = width;
    it->height = height;
    it->size = data_size;
    strncpy(it->id, filename, filename_len);
    return it;
}

xbm_icon_t *
open_xbm (char *filename)
{
    /* See if the data is already stored */
    for (int i=0;i<xiconcache.count;i++)
        if (strncmp(xiconcache.icon[i]->id, filename, MAX_ICON_ID_LEN) == 0)
            return xiconcache.icon[i];

    FILE *f = NULL;
    /* Try to open file */
    if ((f = fopen(filename, "r")) == NULL)
        return NULL;

    char line[MAX_ICON_LINE_LEN];
    char info[MAX_ICON_LINE_LEN];
    char *lp = NULL;
    int width = -1;
    int height = -1;
    int value = -1;
    unsigned int data_size = 0;

    #define DO_FAIL() { if (f) fclose(f); return NULL; }

    while (fgets(line, MAX_ICON_LINE_LEN, f)) {
        /* Check for the height and width defines */
        if (sscanf(line, "#define %s %d", info, &value) == 2) {
            if (!(lp = strrchr(info, '_')))
                lp = info;
            if (strncmp(lp, "_width", 6) == 0)
                width = value;
            else if (strncmp(lp, "_height", 7) == 0)
                height = value;

            continue;
        }
        /* Invalid file, dimensions not set */
        if (width <= 0 || height <= 0)
            DO_FAIL();
        /* Check for the data line next, if not found then give up */
        if (sscanf(line, "static char %s[] = {", info) == 1)
            ;
        else if (sscanf(line, "static unsigned char %s[] = {", info) == 1)
            ;
        else if (sscanf(line, "static const unsigned char %s[] = {", info) == 1)
            ;
        else if (sscanf(line, "static const char %s[] = {", info) == 1)
            ;
        else
            DO_FAIL();

        break;
    }
    /* alloc icon struct with empty data and add to cache */
    xbm_icon_t *it = create_xbm_icon(width, height, filename);
    if (it == NULL)
        DO_FAIL();
    /* Read data from xbm file */
    if (!read_xbm_data(f, it->size, info, it->data))
        DO_FAIL();
    if (f)
        fclose(f);
    return it;
}
