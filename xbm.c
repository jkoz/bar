#include <xcb/xcb_image.h>

#define MAX_LEN 256
#define MAX_ICON_CACHE 32
#define MAX_ICON_SIZE 1024

typedef struct xbm_icon_t {
    int width, height, len;
    char *id;
    unsigned char *data;
} xbm_icon_t;

typedef struct xbm_icon_cache_t {
    int pos;
    int count;
    struct xbm_icon_t *icon[MAX_ICON_CACHE];
} xbm_icon_cache_t;

static xbm_icon_cache_t xiconcache;

int
xbm_icon_add (const int width, const int height, char *filename, unsigned char *data)
{
    xbm_icon_t *it = NULL;

    if (width > 0 && height > 0 && filename != NULL && data != NULL) {
        /* Seen alternative equation (width/8 + !!(width%8))*height */
        /* Haven't seen the purpose yet so haven't added */
        const int n = width/8*height;
        it = calloc(1, sizeof(xbm_icon_t));
        if (!it) {
            fprintf(stderr, "Failed to allocate new icon\n");
            exit(EXIT_FAILURE);
        }
        int pos = xiconcache.pos;
        if (pos == MAX_ICON_CACHE) {
            pos = 0;
        } else {
            /* Not tested yet, increments the count of items in cache if
                    the cache hasn't rolled back yet */
            xiconcache.count++;
        }
        xiconcache.icon[pos] = it;

        it->width=width;
        it->height=height;
        it->len=n;

        it->id = calloc(strlen(filename), sizeof(char));
        if (!it->id) {
            fprintf(stderr, "Failed to allocate new icon\n");
            exit(EXIT_FAILURE);
        }
        strcpy(it->id, filename);

        it->data = calloc(n, sizeof(unsigned char));
        if (!it->data) {
            fprintf(stderr, "Failed to allocate new icon\n");
            exit(EXIT_FAILURE);
        }
        for (int i=0;i<n+1;i++) {
            it->data[i] = data[i];
        }

        xiconcache.pos++;
        return pos;
    }

    return -1;
}

bool
read_xbm_data (FILE *f, const int n, char* info, unsigned char *data)
{
    char *lp = NULL;
    unsigned value;

    if (!(lp = strrchr(info, '_')))
        lp = info;
    /* It is possible that the type will be short instead.
            This could be improved to support those xbm */
    if (strncmp(lp, "_bits[]", 7) == 0) {
        for (int i=0;i<n;i++) {
            if (i > 0) {
                fgetc(f);
            }
            if (fscanf(f, "%x", &value) != 1 || value > 255) {
                return false;
            }
            data[i] = value;
        }
        return true;
    }
    return false;
}

int
check_xbm_cache (char *filename)
{
    for (int i=0;i<xiconcache.count;i++) {
        if (strncmp(xiconcache.icon[i]->id, filename, MAX_LEN) == 0) {
            return i;
        }
    }
    return -1;
}

xbm_icon_t *
open_xbm (char *filename)
{
    int offset = -1;
    /* See if the data is already stored */
    if ((offset = check_xbm_cache(filename)) > -1) {
        return xiconcache.icon[offset];
    }

    FILE *f = NULL;
    f = fopen(filename, "r");
    if (!f) {
        return NULL;
    }

    int width = -1;
    int height = -1;
    unsigned char data[MAX_ICON_SIZE];
    char line[MAX_LEN];
    char info[MAX_LEN];
    char *lp = NULL;
    int value;
    xbm_icon_t *icon = NULL;

    while (fgets(line, MAX_LEN, f)) {
        /* Check for the height and width defines */
        if (sscanf(line, "#define %s %d", info, &value) == 2) {
            if (!(lp = strrchr(info, '_')))
                lp = info;
            if (strncmp(lp, "_width", 6) == 0) {
                width = value;
            } else if (strncmp(lp, "_height", 7) == 0) {
                height = value;
            }
            continue;
        }
        if (width <= 0 || height <= 0) {
            /* Invalid file */
            if (f)
                fclose(f);
            return NULL;
        }

        /* Check for the data line next, if not found then give up */
        if (sscanf(line, "static char %s[] = {", info) >= 1)
            ;
        else if (sscanf(line, "static unsigned char %s[] = {", info) == 1)
            ;
        else if (sscanf(line, "static const unsigned char %s[] = {", info) == 1)
            ;
        else if (sscanf(line, "static const char %s[] = {", info) == 1)
            ;
        else {
            if (f)
                fclose(f);
            return NULL;
        }

        /* Seen alternative equation (width/8 + !!(width%8))*height */
        /* Haven't seen the purpose yet so haven't added */
        const int n = width/8*height;

        if (n > MAX_ICON_SIZE-1)
            return false;

        if (!read_xbm_data(f, n, info, data)) {
            if (f) {
                fclose(f);
                return NULL;
            }
        }
        break;
    }

    int pos;
    if ((pos = xbm_icon_add(width, height, filename, data)) < 0) {
        if (f)
            fclose(f);
            return NULL;
    }

    if (f)
        fclose(f);
    return xiconcache.icon[pos];
}

void
xbm_icon_cache_free (void)
{
    for (int i=0;i<MAX_ICON_CACHE;i++) {
        if (xiconcache.icon[i]) {
            if (xiconcache.icon[i]->id != NULL) {
                free(xiconcache.icon[i]->id);
            }
            if (xiconcache.icon[i]->data != NULL) {
                free(xiconcache.icon[i]->data);
            }
            free(xiconcache.icon[i]);
        }
    }
}

void
xbm_init (void)
{
    memset(&xiconcache, 0, sizeof(xbm_icon_cache_t));
}

void
xbm_cleanup (void)
{
    xbm_icon_cache_free();
}
