$s='src\server.hpp'; $t=[IO.File]::ReadAllText($s); $t=$t.Replace('JpegWallLock wall_lock;','std::lock_guard<std::mutex> lk(jpeg_hw_mutex());'); [IO.File]::WriteAllText($s,$t)
$p='src\task.hpp'; $t=[IO.File]::ReadAllText($p); $t=$t.Replace("            jpeg_yield_to_wall();`r`n",""); $t=$t.Replace("                jpeg_yield_to_wall();`r`n",""); [IO.File]::WriteAllText($p,$t)
$c='src\common.hpp'; $t=[IO.File]::ReadAllText($c)
$start=$t.IndexOf('// The 10 fps merge wall must not queue')
$end=$t.IndexOf('};',$t.IndexOf('struct JpegWallLock'))+2
if($start -ge 0 -and $end -gt $start){ $t=$t.Substring(0,$start)+$t.Substring($end) }
[IO.File]::WriteAllText($c,$t)
