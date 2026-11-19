$m=(Select-String -Path src\task.hpp -Pattern 'jpeg_yield_to_wall' | Measure-Object).Count
$n=(Select-String -Path src\common.hpp -Pattern 'jpeg_wall_priority|JpegWallLock' | Measure-Object).Count
echo "task_yield=$m common_prio=$n"
