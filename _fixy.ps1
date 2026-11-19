$p='src\task.hpp'; $t=[IO.File]::ReadAllText($p)
$bad='jpeg_yield_to_wall();"`r`n"            std::lock_guard<std::mutex> lk(jpeg_hw_mutex());'
$good="jpeg_yield_to_wall();`r`n            std::lock_guard<std::mutex> lk(jpeg_hw_mutex());"
$t=$t.Replace($bad,$good)
[IO.File]::WriteAllText($p,$t)
