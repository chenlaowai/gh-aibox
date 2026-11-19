$p='src\routeb\compat.hpp'; $t=[IO.File]::ReadAllText($p)
$old='                if (bm_image_alloc_contiguous_mem_heap_mask(1, &nv12, 6) == BM_SUCCESS) {'
$new='                if (bm_image_alloc_dev_mem(nv12, 1) == BM_SUCCESS) {'
$t=$t.Replace($old,$new)
$t=$t.Replace('                    if (made) {
                        bm_image_free_contiguous_mem(1, &nv12);
                        bm_image_destroy(nv12);
                    }','                    if (made) bm_image_destroy(nv12);')
$t=$t.Replace('                if (made) {
                    bm_image_free_contiguous_mem(1, &nv12);
                    bm_image_destroy(nv12);
                }','                if (made) bm_image_destroy(nv12);')
[IO.File]::WriteAllText($p,$t)
