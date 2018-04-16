#!/bin/bash
# Script for testing socket assignment 2.3

SOURCE_DIR="source"
GCC_OUTPUT="gcc_output.txt"
TOOLS_DIR="tools"

# These names should include only alphanumeric characters and underscores
PORTFINDER="port_finder"

SERVER1="server1_sol"
SERVER1_DIR="server1"
CLIENT1="client1_sol"
CLIENT1_DIR="client1"

TESTCLIENT1="client1_ref"
TESTSERVER1="server1_ref"

TEST_FILE="small_file1.txt"


# Command to avoid infinite loops
TIMEOUT="timeout"

# Maximum allowed running time in seconds of a standard client (to avoid infinite loops)
MAX_EXEC_TIME=20

# Maximum allowed allocated memory of a standard client (KB)
MAX_ALLOC_MEM=524288

# maximum time in seconds that the script will allow for servers to start
MAX_WAITING_TIME=5	# This parameter must be a positive integer


#*******************************KILL PROCESSES***********************************************************
# Kill al the client and server processes that are still running
function killProcesses
{
    killall -9 $SERVER1 $TESTSERVER1 $CLIENT1 $TESTCLIENT1 2>&1 &> /dev/null
}


#**********************************CLEANUP***************************************************************
function cleanup
{
    echo -n "Cleaning up..."

    # Kill pending processes
    killProcesses

    # Delete previously generated folders and files (if they exist)
    rm -r -f temp*  /tmp/temp* 2>&1 &> /dev/null   # temporary file names start all with temp
    rm -f $SOURCE_DIR/$SERVER1 2>&1 &> /dev/null
    rm -f $SOURCE_DIR/$CLIENT1 2>&1 &> /dev/null

    echo -e "\t\t\tOk!"
}

#***************************CHECK TESTING TOOLS**********************************************************
# check files necessary for tests are present
function checkTools
{
    echo -n "checking all necessary files are present in tools folder..."

    if [ ! -e $TOOLS_DIR/$PORTFINDER ]\
	|| [ ! -e $TOOLS_DIR/$TEST_FILE ]\
        || [ ! -e $TOOLS_DIR/$TESTSERVER1 ]\
	|| [ ! -e $TOOLS_DIR/$TESTCLIENT1 ]\
        ; then
            echo -e "\tFail!. \n[ERROR] Could not find testing tools or missing test files. Test aborted!"
            exit -1
    else
        echo -e "\tOk!"
    fi

}

#*******************************SINGLE COMPILATION*******************************************************
# Performs a single compilation command
# Arguments:
# $1: the executable name
# $2: the folder where the program to compile is
function gcc_compile
{
    gcc -std=gnu99 -o $1 $2/*.c *.c -I$2 -lpthread -lm >> $GCC_OUTPUT 2>&1
}


#********************************COMPILE SOURCES*********************************************************
# compiles all sources
function compileSource
{
    cd $SOURCE_DIR
    rm -f $GCC_OUTPUT

    echo -n "Test 0.1 (compiling $SERVER1)...";
    gcc_compile $SERVER1 $SERVER1_DIR
    if [ ! -e $SERVER1 ] ; then
        TEST_01_PASSED=false
        echo -e "\tFail! \n\t[ERROR] Unable to compile source code for $SERVER1."
        echo -e "\tGCC log is available in $SOURCE_DIR/$GCC_OUTPUT"
    else
        TEST_01_PASSED=true
        echo -e "\tOk!"
    fi

    echo -n "Test 0.2 (compiling $CLIENT1)...";
    gcc_compile $CLIENT1 $CLIENT1_DIR
    if [ ! -e $CLIENT1 ] ; then
	TEST_02_PASSED=false
	echo -e "\tFail! \n\t[ERROR] Unable to compile source code for $CLIENT1."
	echo -e "\tGCC log is available in $SOURCE_DIR/$GCC_OUTPUT"
    else
	TEST_02_PASSED=true
	echo -e "\tOk!"
    fi

    cd ..
}

#*************************************GET MEMORY USAGE***************************************************
# Get the total memory usage of all processes that match a given pattern
# Arguments:
# $1: the pattern of the processes to check
function getMemUsage
{
    tot_mem=0
    for f in `ps -ef | grep $1 | awk '{print $2}'`;
    do
            mem=$(pmap -x $f | tail -1 | awk '{print $3}')
            if [[ ! -z "$mem" ]] ; then
                    tot_mem=$(($tot_mem+$mem))
            fi
    done
    echo $tot_mem
}

#*************************************WATCHDOG***********************************************************
# Kill a process after the timeout expires or the memory threshold is reached
# Arguments:
# $1: the name of the process to watch
# $2: the timeout
# $3: the memory threshold
function watchdog
{
    for (( i=0; i<=$(($2*10)); i++ ))
    do
        sleep 0.1
        mem_usage=$(getMemUsage $1)
        if (( $mem_usage>$3 )) ; then
            echo "killing $1 as it allocated $mem_usage KB of memory"
            killall -9 $1 2>&1 &> /dev/null
            return
        fi
    done
    echo "killing $1 as it exceeded execution timeout"
    killall -9 $1 2>&1 &> /dev/null
}

#*************************************SETUP TEMP DIRECTORY FOR SERVER************************************
# Prepares the temp directory of the server
# Arguments:
# $1: the name of the server executable file
# $2: the directory to be used for the server
function setupTempServerDir
{
    # Creating empty temp directory for server
    rm -r -f $2
    mkdir $2 2>&1 &> /dev/null

    # Copying test-related files to temp directory
    cp -f $TOOLS_DIR/$PORTFINDER $2 2>&1 &> /dev/null
    cp -f $TOOLS_DIR/$TEST_FILE $2 2>&1 &> /dev/null

    # Check if we are testing the student server
    if [[ $1 == $SERVER1 ]] ; then
        testStudentServer=true
    else
        testStudentServer=false
    fi

    # Copying the server executable file to the temp directory
    if [[ $testStudentServer == true ]] ; then
        cp -f $SOURCE_DIR/$1 $2 2>&1 &> /dev/null
    else
        cp -f $TOOLS_DIR/$1 $2 2>&1 &> /dev/null
fi
}

#*************************************SETUP TEMP DIRECTORY FOR CLIENT************************************
# Prepares the temp directory of the client
# Arguments:
# $1: the name of the client executable file
# $2: the directory to be used for the client
function setupTempClientDir
{
    # Creating empty temp directory for client
    rm -r -f $2
    mkdir $2 2>&1 &> /dev/null


    # Check if we are testing the student client
    if [[ $1 == $CLIENT1 ]] ; then
        testStudentClient=true
    else
        testStudentClient=false
    fi

    # Copying the client executable file to the temp directory
    if [[ $testStudentClient == true ]] ; then
        cp -f $SOURCE_DIR/$1 $2 2>&1 &> /dev/null
    else
        cp -f $TOOLS_DIR/$1 $2 2>&1 &> /dev/null
    fi
}


#************************************RUN SERVER**********************************************************
# Runs the specified server in the specified directory
# Arguments:
# $1: the server to be run (including path relative to server directory)
# $2: the directory of the server to be run
# $3: the file for recording the server standard output and error
# Return value: the listening port or -1 in case of error
function runServer
{
    pushd $2 >> /dev/null
    local FREE_PORT=`./$PORTFINDER`     # This will find a free port
    $1 $FREE_PORT &> $3 &               # Launch server with free port on cmd line
    res=$(ensureServerStarted $FREE_PORT)
    if [[ $res != "0" ]] ; then
        echo "-1"
        return
    fi
    popd >> /dev/null
    echo $FREE_PORT     # Returning to the caller the port on which the server is listening
}

#************************************RUN CLIENT**********************************************************
# Runs the specified client in the current directory
# Assumption: the client accepts server name or address and port number as first params
# Arguments:
# $1: the client to be run (with path relative to client directory). Assumption: starts with "./"
# $2: the directory of the client to be run
# $3: the address or name of the server
# $4: the port number of the server
# $5...: the last parameters accepted by the client
# Global variables used by this function:
# $TIMEOUT:          timeout command
# $MAX_EXEC_TIME:    timeout value
# $client_run:       progressive number of client run
# $test_suite:       progressive number of test suite
# $testStudentClient:true if we are testing the student client
function runClient
{
    local cli=$1
    local cli_exe=${1:2}	# eliminate leading "./" to get client exec name only
    local cli_dir=$2
    local srv=$3
    local port=$4
    shift 4
    echo -n "Running client $cli (server $srv, port $port, next arguments: $@) ..."
    pushd $cli_dir >> /dev/null

    # start the watchdog for the client to run
    if [[ $testStudentClient == true ]] ; then
        echo -e "\nStarting watchdog for ${cli_exe} ..."
        watchdog ${cli_exe} $MAX_EXEC_TIME $MAX_ALLOC_MEM &	
        wd_pid=$!
    fi

    # run the client
        $TIMEOUT $MAX_EXEC_TIME $cli "$srv" "$port" ${@} > output_tmp${cli_exe} &

    # wait for client to finish
    wait $!
    # get return value
    rc=$?
    # get output
    read output < output_tmp${cli_exe}
    rm output_tmp${cli_exe}

    # kill the watchdog
    if [[ $testStudentClient == true ]] ; then
        echo "killing watchdog"
        kill -9 $wd_pid
	wait $wd_pid 2>&1 &> /dev/null 	# just to suppress "Killed" message
    fi

    echo -e "\nExecution of client $cli completed."

    # perform test
    client_run=$(($client_run + 1 ))
    # store std out and return value
    outputname=${cli_exe}"output$test_suite$client_run"
    eval ${outputname}="'$output'"
    returnname=${cli_exe}"return$test_suite$client_run"
    eval ${returnname}="'$rc'"
    # give message about exit code of client
    if [[ $testStudentClient == false ]] && (( $rc == 1 )) ; then
        echo -e "\t$1 could not connect to $srv at port $port"
    elif [[ $testStudentClient == false ]] && (( $rc == 2 )) ; then
        echo -e "\tBad response received from $srv (or timeout)"
    elif [[ $testStudentClient == false ]] && (( $rc == 3 )) ; then
	    echo -e "\tError code received from $srv"
    elif [[ $testStudentClient == false ]] && (( $rc == 4 )) ; then
	    echo -e "\tTimeout while receiving file from $srv"
    elif [[ $testStudentClient == false ]] && (( $rc == 5 )) ; then
        echo -e "\tTest client run terminated successfully"
    else
        echo -e "\tClient run terminated with exit code $rc"
    fi

    popd >> /dev/null
}


#************************************ENSURE SERVER STARTED***********************************************
# Check a server is listening on the specified port. Wait for at most MAX_WAITING_TIME seconds
# (hardcoded in the script)
# Arguments:
# $1: the port to be checked
#
# Returns 0 for success, -1 otherwise.
# Global variables used by this function:
# $MAX_WAITING_TIME: max waiting time for server to start

function ensureServerStarted
{
	# Ensure the server has started
	for (( i=1; i<=$MAX_WAITING_TIME; i++ ))	# Maximum MAX_WAITING_TIME tries
	do
		#fuser $1/tcp &> /dev/null
		rm -f temp
		netstat -ln | grep "tcp" | grep ":$1 " > temp
		if [[ -s temp ]] ; then
			# Server started
			rm -f temp
			echo "0"
			return
		else
			# Server didn't start
			sleep 1
		fi
	done
	rm -f temp
	echo "-1"
}

#*************************************TEST CLIENT-SERVER INTERACTION*************************************
# Runs the specified server and client and performs a number of tests
# Arguments:
# $1: server to be run (name of executable file)
# $2: client to be run (name of executable file)
#
function testClientServerInteraction
{
    # Setup temp directory for server
    setupTempServerDir "$1" "/tmp/temp_server_dir_$$"	# we append pid ($$) to fixed dir name

    # Setup temp directory for client
    setupTempClientDir "$2" "/tmp/temp_client_dir_$$"

    # Run server
    echo -e "Running server $1 ...\n\n"
    server_port=$(runServer "./$1" "/tmp/temp_server_dir_$$" "$1output$test_suite.txt")
    if [[ $server_port -eq "-1" ]] ; then
        echo -e "Server did not start."
        echo -e "\tSkipping the next tests of this part"
        test_number=5
        return
    else
        echo -e "\t $1 [PORT $server_port] Ok!"
    fi

    # Run client with small file
    local fname=$TEST_FILE
    runClient "./$2" "/tmp/temp_client_dir_$$" "127.0.0.1" "$server_port" "$fname"

    # TEST return value of test client (>=5)
    # TEST 1.1
    if [[ $testStudentClient == false ]] ; then
         testClientReturnValue $2 "5" "-ge"
         if [[ $result == "failed" ]] ; then
           echo -e "\tSkipping the next tests of this part"
           test_number=5
           return
         fi
    fi

    # TEST the requested file is present in the client dir and is identical to the expected one
    # TEST 1.2
    testFilesEquality "/tmp/temp_server_dir_$$" "/tmp/temp_client_dir_$$" "$fname"

}

#************************************Get return value of test client*************************************
# returns (to the standard output) the client return value
# Arguments:
# $1: the client executable
#
function getClientReturnValue
{
    retvalname="$1return$test_suite$client_run"
    echo "${!retvalname}"
}

#************************************Test return value of test client************************************
# Check the client return value is equal to the expected one
# Stores result ("passed" or "failed") in global var $result
# Arguments:
# $1: the client executable
# $2: the expected return value
# $3: if given and equal to "-ge", passes also for values higher than the expected one
#
function testClientReturnValue
{
    test_number=$(($test_number + 1))
    rc=$(getClientReturnValue $1)
    if [[ $2 == 2 ]] ; then
        echo -e "\n\n[TEST $test_suite.$test_number] Checking if the interaction between client and server has been started correctly ..."
    elif [[ $2 == 3 ]] ; then
        echo -e "\n\n[TEST $test_suite.$test_number] Checking if the interaction between client and server has been completed correctly ..."
    elif [[ $2 == 1 ]] ; then
        echo -e "\n\n[TEST $test_suite.$test_number] Checking if the client recognized the timeout ..."
    fi
    local tname="TEST_$test_suite$test_number"
    local tname+="_PASSED"
    if [[ ( $# == 2 && $rc == $2 ) || ( $# == 3 && $3 == "-ge" && $rc -ge $2 ) ]]; then
        echo -e "\t[++TEST $test_suite.$test_number PASSED++] "
        eval ${tname}=true
        result=passed
    else
        echo -e "\t[--TEST $test_suite.$test_number FAILED--]"
        eval ${tname}=false
        result=failed
    fi
}

#************************************Test file present and identical*************************************
# Check the expected file is present in the client directory and equals the expected one
# Stores result ("passed" or "failed") in global var $result
# Arguments:
# $1: the server directory
# $2: the client directory
# $3: the name of the file to test
#
function testFilesEquality
{
    test_number=$(($test_number + 1))
    echo -e "\n\n[TEST $test_suite.$test_number] Checking the transferred file exists in client directory and equals the original ones ..."
    local tname="TEST_$test_suite$test_number"
    local tname+="_PASSED"
    fequality=$(getFileEquality "$1" "$2" "$3")
    if [[ $fequality == "0" ]] ; then
        echo -e "\n\t[++TEST $test_suite.$test_number PASSED++] Ok!"
        eval ${tname}=true
        result=passed
    elif [[ $fequality == "1" ]] ; then
        echo -e "\n\t[--TEST $test_suite.$test_number FAILED--] File is not present in the client directory."
        eval ${tname}=false
        result=failed
    elif [[ $fequality == "2" ]] ; then
        echo -e "\n\t[--TEST $test_suite.$test_number FAILED--] Files are not equal."
        eval ${tname}=false
        result=failed
    fi
}

#************************************Get info about file present and identical***************************
# Test if the expected file is present in the client directory and equals the expected one
# (with same name in $TOOLS_DIR directory
# Arguments:
# $1: the server directory
# $2: the client directory
# $3: the name of the file to test
#
# Returns 0 for test success, 1 if file not present, 2 if file not equal
function getFileEquality
{
    if [ -e $2/$3 ] ; then
        diff "$1/$3" "$2/$3" 2>&1 &> /dev/null
        rc=$?
        if [ $rc -eq 0 ] ; then
            echo "0"
        else
            echo "2"
        fi
    else
        echo "1"
    fi
}



#********************************** TEST INITIALIZATION *************************************************

cleanup
checkTools
compileSource


#********************************** TEST SUITE 1 ********************************************************
echo -e "\n*** PART 1: TESTS ON THE STUDENT'S SERVER WITH A REFERENCE CLIENT *************"
test_suite=1
test_number=0
client_run=0

if [[ $TEST_01_PASSED == true ]] ; then

    testClientServerInteraction "$SERVER1" "$TESTCLIENT1"
    killProcesses
else
	echo -e "---Skipping test of Part 1 because the student's server didn't compile---"
fi
echo -e "*** END OF TESTING PART 1 *************"

#********************************** TEST SUITE 2 ********************************************************
echo -e "\n\n*** PART 2: TESTS ON THE STUDENT'S CLIENT WITH THE STUDENT'S SERVER *************"
test_suite=2
test_number=0
client_run=0

if [[ $TEST_01_PASSED == true ]] && [[ $TEST_02_PASSED == true ]] ; then

    testClientServerInteraction "$SERVER1" "$CLIENT1"
    killProcesses
else
	echo -e "---Skipping test of Part 2 because the student's server or client didn't compile---"
fi
echo "*** END OF TESTING PART 2 *************"

#********************************** TEST SUITE 3 ********************************************************
echo -e "\n\n*** PART 3: TESTS ON THE STUDENT'S CLIENT WITH A REFERENCE SERVER *************"
test_suite=3
test_number=0
client_run=0

if [[ $TEST_02_PASSED == true ]] ; then

    testClientServerInteraction "$TESTSERVER1" "$CLIENT1"
    killProcesses
else
	echo -e "---Skipping test of Part 3 because the student's client didn't compile---"
fi
echo "*** END OF TESTING PART 3 *************"


#****************************** SUMMARY *****************************************************************

# recap print
echo ""
echo " ----- Tests recap ------"
echo " - test 0.1: "${TEST_01_PASSED:-skipped}	#prints "skipped" if variable was not set
echo " - test 0.2: "${TEST_02_PASSED:-skipped}
echo ""
echo " - test 1.1: "${TEST_11_PASSED:-skipped}
echo " - test 1.2: "${TEST_12_PASSED:-skipped}
echo ""
echo " - test 2.1: "${TEST_21_PASSED:-skipped}
echo ""
echo " - test 3.1: "${TEST_31_PASSED:-skipped}
echo " ------------------------"

# Checking minimum requirements

npassed=0
nmessages=0
passed=false

if [[ $TEST_11_PASSED == true ]] ; then
	((npassed++))
elif [[ $TEST_11_PASSED == false ]]; then
	((nmessages++))
	echo -e "\n \t The student's server could not be contacted or did not respond correctly"
fi

if [[ $TEST_12_PASSED == true ]]; then
	((npassed++))
elif [[ $TEST_12_PASSED == false ]]; then
	((nmessages++))
	echo -e "\n \t The student's server failed to deliver the file as expected"
fi

if [[ $TEST_21_PASSED == true ]]; then
	((npassed++))
elif [[ $TEST_21_PASSED == false ]]; then
	((nmessages++))
	echo -e "\n \t The interaction between the student's client and server did not work correctly"
fi

if [[ $TEST_31_PASSED == true ]]; then
	((npassed++))
elif [[ $TEST_31_PASSED == false ]]; then
	((nmessages++))
	echo -e "\n \t The interaction between the student's client and the reference server did not work correctly"
fi
if [[ $npassed == 4 ]] ; then
	passed=true
else
	passed=false
fi
if [[ $passed == false ]] ; then
	echo -e "\n----- FAIL: You have not met the minimum requirements for submission!!! -----\n"
else
	echo -e "\n+++++ OK: You may have met the minimum requirements for submission!!! +++++\n"
fi
if [[ "$nmessages" -ge 1 ]] ; then
	echo -e "\n### Fix the reported errors to meet the minimum requirements ###\n "
fi

#**************************************** CLEANUP *******************************************************
cleanup

#**************************************** EXIT *******************************************************
if [[ $passed == "true" ]] ; then
	exit 0
else
	exit 1
fi
