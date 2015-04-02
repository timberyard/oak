oak - Build tool
================

Build the build tool
--------------------

cd src ; cmake . ; make


Run the build tool
------------------

See ```oak -h``` for current command-line options.

Now I describe oak version 0.4.

There are two *modes of operatons*: “direct mode” and "Jenkins mode”. In “direct mode“ all required options are given on command-line directly. In “Jenkins mode” the option values come from environment variables.


| *command line option* | *environment variable* | *description* |
|-----------------------|------------------------|---------------|
| -i  or  --input       | WORKSPACE              |  input_directory – the directory where the program being built is checked out into |
| -o  or  --output      |                        |  output_directory – where all the built artifacts are stored into |
| -c  or  --config      |                        |  the configration JSON file |
| -r  or  --result      |                        |  the name of the JSON file where the result is written into |
| -M  or  --machine     | NODE_NAME              |  name of the node where the built runs |
| -R  or  --repository  | GIT_URL                |  the URL of the git repository |
| -B  or  --branch      | GIT_BRANCH             |  guess what |
| -C  or  --commit      | GIT_COMMIT             |  the git commit ID |
| -T  or  --timestamp   | BUILD_ID               |  the timestamp of the commit = ID of the build |
| 


Use the remove old builds cron script
-------------------------------------

Touch a .branch file in every branch directory (the directories containing the folders with the build date and time). Then add del_old_builds.sh to your crontab. It searches for the .branch files and removes all but the last 3 build folders next to them.

