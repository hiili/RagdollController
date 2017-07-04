#!/bin/bash

declare -A ue4editor
ue4editor_path="C:/Program Files/Epic Games/UE_4.16/Engine/Binaries/Win64"
ue4editor[debug]="$ue4editor_path/UE4Editor-Win64-Debug.exe"
ue4editor[debuggame]="$ue4editor_path/UE4Editor.exe"
ue4editor[development]="$ue4editor_path/UE4Editor.exe"

ue4project=RagdollController.uproject

declare -A args
args[editor]="-log -UseFixedTimeStep"
args[standalone]="-game -log -UseFixedTimeStep"
args[server]="-server -game -log -UseFixedTimeStep"
args[listenserver]="main?listen -game -log -UseFixedTimeStep"
args[client]="127.0.0.1 -game -log -UseFixedTimeStep"

declare -A args_debug
args_debug[debug]="-debug"
args_debug[debuggame]="-debug"
args_debug[development]=" "

if [[ $# != 2 ]] ; then
	echo "Usage: launch.bash debug|debuggame|development editor|standalone|server|listenserver|client"
	exit 1
fi

if [[ -z "${ue4editor[$1]}" ]] || [[ -z "${args[$2]}" ]] || [[ -z "${args_debug[$1]}" ]]; then
	echo "Invalid arguments!"
	exit 1
fi


dos_wd=$(echo $PWD | sed 's/.cygdrive.\([a-z]\)/\1:/')

echo ${ue4editor[$1]} "\"$dos_wd/$ue4project\"" ${args[$2]} ${args_debug[$1]}
cmd /C "${ue4editor[$1]}" $dos_wd/$ue4project ${args[$2]} ${args_debug[$1]}
