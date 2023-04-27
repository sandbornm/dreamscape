# load the module
module_name="cache_kprobe_monitor_v2.ko"

cd "../cache_kprobe_monitor_v2/"

if lsmod | grep -q $module_name; then
  echo "$module_name is already loaded."
else
  sudo insmod $module_name
  echo "$module_name has not been loaded."
  
fi

binary_path="/home/ubuntu/dreamscape/test_bins/bins/arm64/onebil_memacc"
echo "runnning $binary_path"
nohup $binary_path > /dev/null 2>&1 &
pgm_pid=$!

timestamp=$(date +"%Y%m%d_%H%M%S")

echo $binary_path,$pgm_pid,$timestamp >> ./binpath_pid_record 
echo $pgm_pid | sudo tee /proc/cache_kmv2_pid

# watch dmesg and run dump_dmesg.sh
# run test module multiple times
