typedef struct {
    int valid;
    int frame;
    int dirty;
    int requested;
    int last_accessed_at;
} page_table_entry;

