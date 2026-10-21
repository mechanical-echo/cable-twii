#include "nav.h"

#include <stdlib.h>

static int cmp_int(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

int nav_step(const ctw_db *db, int current_number, int dir)
{
    size_t n = ctw_db_channel_count(db);
    if (n == 0) return current_number;

    int *nums = (int *)malloc(n * sizeof(int));
    if (nums == NULL) return current_number;
    for (size_t i = 0; i < n; i++) {
        nums[i] = ctw_db_channel_at(db, i)->number;
    }
    qsort(nums, n, sizeof(int), cmp_int);

    size_t idx = 0;
    bool found = false;
    for (size_t i = 0; i < n; i++) {
        if (nums[i] == current_number) {
            idx = i;
            found = true;
            break;
        }
    }

    int result;
    if (found) {
        size_t next = dir > 0 ? (idx + 1) % n : (idx + n - 1) % n;
        result = nums[next];
    } else if (dir > 0) {
        result = nums[0];
        for (size_t i = 0; i < n; i++) {
            if (nums[i] > current_number) {
                result = nums[i];
                break;
            }
        }
    } else {
        result = nums[n - 1];
        for (size_t i = n; i-- > 0;) {
            if (nums[i] < current_number) {
                result = nums[i];
                break;
            }
        }
    }

    free(nums);
    return result;
}
