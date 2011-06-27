##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Debug
ProjectName            :=moodpd_codelite
ConfigurationName      :=Debug
IntermediateDirectory  :=./Debug
OutDir                 := $(IntermediateDirectory)
WorkspacePath          := "/home/johannes/code/tests/moodpd_test"
ProjectPath            := "/home/johannes/code/moodpd/proj"
CurrentFileName        :=
CurrentFilePath        :=
CurrentFileFullPath    :=
User                   :=johannes
Date                   :=06/26/2011
CodeLitePath           :="/home/johannes/.codelite"
LinkerName             :=g++
ArchiveTool            :=ar rcus
SharedObjectLinkerName :=g++ -shared -fPIC
ObjectSuffix           :=.o
DependSuffix           :=.o.d
PreprocessSuffix       :=.o.i
DebugSwitch            :=-gstab
IncludeSwitch          :=-I
LibrarySwitch          :=-l
OutputSwitch           :=-o 
LibraryPathSwitch      :=-L
PreprocessorSwitch     :=-D
SourceSwitch           :=-c 
CompilerName           :=g++
C_CompilerName         :=gcc
OutputFile             :=$(IntermediateDirectory)/$(ProjectName)
Preprocessors          :=
ObjectSwitch           :=-o 
ArchiveOutputSwitch    := 
PreprocessOnlySwitch   :=-E 
ObjectsFileList        :="/home/johannes/code/moodpd/proj/moodpd_codelite.txt"
MakeDirCommand         :=mkdir -p
CmpOptions             := -g $(Preprocessors)
C_CmpOptions           := -g $(Preprocessors)
LinkOptions            :=  
IncludePath            :=  $(IncludeSwitch). $(IncludeSwitch). 
RcIncludePath          :=
Libs                   :=
LibPath                := $(LibraryPathSwitch). 


##
## User defined environment variables
##
CodeLiteDir:=/usr/share/codelite
Objects=$(IntermediateDirectory)/moodpd_main$(ObjectSuffix) 

##
## Main Build Targets 
##
all: $(OutputFile)

$(OutputFile): makeDirStep $(Objects)
	@$(MakeDirCommand) $(@D)
	$(LinkerName) $(OutputSwitch)$(OutputFile) $(Objects) $(LibPath) $(Libs) $(LinkOptions)

objects_file:
	@echo $(Objects) > $(ObjectsFileList)

makeDirStep:
	@test -d ./Debug || $(MakeDirCommand) ./Debug

PreBuild:


##
## Objects
##
$(IntermediateDirectory)/moodpd_main$(ObjectSuffix): ../main.cpp $(IntermediateDirectory)/moodpd_main$(DependSuffix)
	$(CompilerName) $(SourceSwitch) "/home/johannes/code/moodpd/main.cpp" $(CmpOptions) $(ObjectSwitch)$(IntermediateDirectory)/moodpd_main$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/moodpd_main$(DependSuffix): ../main.cpp
	@$(CompilerName) $(CmpOptions) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/moodpd_main$(ObjectSuffix) -MF$(IntermediateDirectory)/moodpd_main$(DependSuffix) -MM "/home/johannes/code/moodpd/main.cpp"

$(IntermediateDirectory)/moodpd_main$(PreprocessSuffix): ../main.cpp
	@$(CompilerName) $(CmpOptions) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/moodpd_main$(PreprocessSuffix) "/home/johannes/code/moodpd/main.cpp"


-include $(IntermediateDirectory)/*$(DependSuffix)
##
## Clean
##
clean:
	$(RM) $(IntermediateDirectory)/moodpd_main$(ObjectSuffix)
	$(RM) $(IntermediateDirectory)/moodpd_main$(DependSuffix)
	$(RM) $(IntermediateDirectory)/moodpd_main$(PreprocessSuffix)
	$(RM) $(OutputFile)


