set --local scriptname (status --current-filename)

set --local help_message "\
$scriptname:
  A simple fish script to print the latest readouts in the minly files

Usage:
  fish get-latest-minly-readout.fish

Options:
  -n, --count
    Number of latest minly files to show output for. Default: 1
  -h, --help
    Show this help message.

Can be run remotely like this:
  ssh pi@lasse-raspberrypi-0-wifi \\
    \"fish ~/sensor-logging/executables/get-latest-minly-readout.fish\"

Also consider using `watch` for an auto-updating display.
"

argparse "n/count=!_validate_int --min 0" "h/help" -- $argv

if [ $status = 0 ]; and [ (count $_flag_help) = 0 ]
  set --local n 1
  if [ (count $_flag_count) = 1 ]
    set n $_flag_count
  end

  set --local readout_names dht22 sensorhub
  set --local minly_path (string join "/" \
    (realpath (dirname $scriptname)) ".." "data" "minly")

  set --local table

  for readout_name in $readout_names
    set --local filenames (ls $minly_path/*$readout_name.csv | tail -n $n)
    set --local kvs
    if [ (count $filenames) > 0 ]
      set --local ks (string trim (head -n 1 $filenames[1] | tr "," "\n"))
      set --local is (seq 1 (count $ks))
      set --append kvs $ks
      for filename in $filenames[-1..1]
        set --local vs (string trim (tail -n 1 $filename | tr "," "\n"))
        for i in $is
          set --local k $ks[$i]
          set --local v $vs[$i]
          if [ $k = "unix_time" ]
            set v (date --date @$v --iso-8601="seconds")
          end
          set kvs[$i] (string join "," $kvs[$i] $v)
        end
      end
    end
    set --append table $kvs
  end

  echo -e (string join "\n" $table) | column -t -s ","
else
  echo -n $help_message
end
