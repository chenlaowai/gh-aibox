$p='src\common.hpp'; $t=[IO.File]::ReadAllText($p)
$new='// JPU encode/decode is shared by all tasks. A single lock was faster than two
// slots on this box: concurrent encodes contended inside the JPU driver and cut
// merge/preview throughput roughly in half, so keep one process-wide lock.
inline std::mutex& jpeg_hw_mutex() {
    static std::mutex m;
    return m;
}'
$start=$t.IndexOf('// BM1684X has two JPU cores.')
$end=$t.IndexOf('}', $t.IndexOf('inline std::mutex& jpeg_hw_mutex', $start))+1
if($start -ge 0 -and $end -gt $start){ $t=$t.Substring(0,$start)+$new+$t.Substring($end) }
[IO.File]::WriteAllText($p,$t)
$s='src\server.hpp'; $t2=[IO.File]::ReadAllText($s)
$t2=$t2.Replace('std::lock_guard<std::mutex> lk(jpeg_hw_mutex(1));','std::lock_guard<std::mutex> lk(jpeg_hw_mutex());')
[IO.File]::WriteAllText($s,$t2)
