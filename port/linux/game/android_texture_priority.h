/* Android CE HUD residency and bounded recovery comparison. */
#ifndef ANDROID_TEXTURE_PRIORITY_H
#define ANDROID_TEXTURE_PRIORITY_H
struct bitmap_data;
struct lruv_cache;
void android_texture_priority_reset(void);
void android_texture_priority_forget(struct bitmap_data *bitmap);
void android_texture_priority_scope(int enabled);
int android_texture_priority_collecting(void);
void android_texture_priority_note(struct bitmap_data *bitmap);
void android_texture_priority_result(struct bitmap_data *bitmap, int ready);
void android_texture_priority_begin(struct lruv_cache *cache);
int android_texture_priority_recover(struct lruv_cache *cache);
#endif
