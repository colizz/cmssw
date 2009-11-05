#! /bin/bash

### $1: file with list of ALCA types you want to process; remember to put for each of them 
###     an ALCARECO style cfg in Alignment/CommonAlignmentProducer/python/ 
#
### $2: number, if greater than 2 it cleans the CASTOR area before starting (optional) 

curdir=$(pwd)

#CASTOR_OUT="/castor/cern.ch/cms/store/user/bonato/CRAFTReproSkims/Craft09/4T/"
ALCAFILELIST=$1
CASTOR_OUT="<CASTOROUT>"
DQM_OUT="${curdir}/MONITORING/DQM/"
MAXEVENTS=18000

curdir=$( pwd )

#check if output directory exists
nsls $CASTOR_OUT
if [ $? -ne 0 ]
then
echo "Output directory: "
echo $CASTOR_OUT
echo "does not exist. Please check the scripts. Exiting."
exit 1
fi


#check if DQM output directory exists
nsls $DQM_OUT
if [ $? -ne 0 ]
then
echo "DQM directory: "
echo $DQM_OUT
echo "does not exist. Please check the scripts. Exiting."
exit 1
fi


## Clean output directory

if [ $# -gt 1 ]
then 

    if [ $2 -gt 2 ]
	then
	
	if [ $(nsls -l $CASTOR_OUT | wc -l ) -gt 1 ] #there is always the dir 'logfiles' 
	    then
	    echo "Cleaning output directory: $CASTOR_OUT"
	    
	    for file in $(nsls $CASTOR_OUT/ | grep "Skimmed" ) 
	      do
#echo
	      rfrm  $CASTOR_OUT/$file
	    done
	    
	    for file in $(nsls $CASTOR_OUT/logfiles/) 
	      do
#echo
	      rfrm  $CASTOR_OUT/logfiles/$file
	    done
	fi
	
    fi #end if $2 > 2
    
fi #end if $# > 2

for ALCATAG in $( cat $ALCAFILELIST  )
do

  echo
  echo "*****************************************"
  echo "*** Starting the ALCATAG: ${ALCATAG}"
  echo "*****************************************"
  echo
#  DAT_FILE="/afs/cern.ch/cms/CAF/CMSALCA/ALCA_TRACKERALIGN/HIP/bonato/DEVEL/HIPWorkflow/ALCARECOskim/v1.4/data/${ALCATAG}.dat"
  DAT_FILE="${curdir}/data/${ALCATAG}.dat"
  TPL_FILE="TkAlCaRecoSkimming.${ALCATAG}.tpl"
  TAG=$ALCATAG #"CRAFT"
  JOBTAG="ALCASkim_"$TAG
  BASE_TPL=$(basename "$TPL_FILE" .tpl)
  echo ""
  echo "I am in $curdir"
  INDEX=1



# echo "*******************************"
# echo "Starting the show"
# echo

  TOTFILES=0
  for i in $( cat $DAT_FILE )
    do

#pick the total nr events in this file from the previously produced nevents.out
    let TOTFILES=TOTFILES+1
    TOTEVTS=$(sed -n $TOTFILES'p' ./data/nevents${ALCATAG}.out)
#echo "The file #$TOTFILES has $TOTEVTS events"

    TOTSPLITS=$(( ( $TOTEVTS / $MAXEVENTS ) +1 ))
    firstev=0
    lastev=-1
    
#echo "I will split it into $TOTSPLITS"
    if [ $TOTSPLITS > 1 ]
	then
	nsplits=1


	while [ $nsplits -le $TOTSPLITS  ]
	  do
#echo "Splitting the file $TOTFILE : $nsplits"
	  firstev=$(( $MAXEVENTS*$(( $nsplits-1 ))+1 ))
	  lastev=$MAXEVENTS    #$(( ($MAXEVENTS*$nsplits) ))
	  JOB=$JOBTAG"_file"$INDEX 
	  CFG_FILE=$BASE_TPL"."$TAG"_cfg."$INDEX".py"
	  replace "<JOB>" $JOB "<INPATH>"  $i  "<INIEVT>" $firstev "<FINEVT>" $lastev "<ALCATAG>" $TAG < $TPL_FILE > $CFG_FILE
	  let INDEX=INDEX+1
	  let nsplits=nsplits+1
	done

    else #file is small and does not contain too many events
	firstev=0
	lastev=-1
	JOB=$JOBTAG"_file"$INDEX 
	CFG_FILE=$BASE_TPL"."$TAG"_cfg."$INDEX".py"
	replace "<JOB>" $JOB "<INPATH>"  $i  "<INIEVT>" $firstev "<FINEVT>" $lastev  "<ALCATAG>" $ALCATAG < $TPL_FILE > $CFG_FILE
	let INDEX=INDEX+1
    fi

#echo "--- moving to next file. At the moment INDEX=$INDEX"


  done

  TOTCFGFILES=$(( $INDEX -1 ))
#echo "Tot cfg files: $TOTCFGFILES"

##second loop submitting what we prepared before
  INDEX=1
  while [ $INDEX -le $TOTCFGFILES ]
    do
    JOBNAME="ALCASkim"$TAG"_"$INDEX
    LOGFILE="${JOBNAME}.log"
    CFG_FILE=$BASE_TPL"."$TAG"_cfg."$INDEX".py"

    
##if [ $INDEX -gt 126 -a $INDEX -lt 133 ] # dummy
##then 
    echo "Submitting $JOBNAME with config file $CFG_FILE"
    if [ $INDEX -lt 100 ]
	then
#	echo "dummy A" 
	bsub -q cmscaf1nd -J $JOBNAME -oo $LOGFILE skim_exec.sh "$curdir/$CFG_FILE" "$CASTOR_OUT" "$DQM_OUT"
    elif [ $INDEX -lt 200 ] 
	then 
#echo "dummy B" 
	bsub -q cmsexpress -J $JOBNAME -oo $LOGFILE skim_exec.sh "$curdir/$CFG_FILE" "$CASTOR_OUT" "$DQM_OUT"
    else
#echo "dummy C" 
	bsub -q cmscaf1nh    -J $JOBNAME -oo $LOGFILE skim_exec.sh "$curdir/$CFG_FILE" "$CASTOR_OUT" "$DQM_OUT"
    fi


##fi #dummy


    let INDEX=INDEX+1
  done




done #end 'for loop' on ALCA categories


#cd $curdir
#mv "$BASE_TPL"*".py" $DQM_OUT/../logfiles/
