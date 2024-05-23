
export VER := 0.18.58
export STANZA_ZIP := stanza.zip

# use a different CONAN_HOME for each target
export CONAN_HOME := ${PWD}/.conan2
$(info CONAN_HOME is ${CONAN_HOME})

# execute all lines of a target in one shell
.ONESHELL:
.SECONDEXPANSION:

platforms := linux macos windows

all: ${platforms}
.PHONY: all ${platforms}

${CONAN_HOME}/profiles/default:
	conan profile detect

${CONAN_HOME}/profiles/%:
	set +x
	 # Capitalize the target platform and create a profile for it
	CAP_PLATFORM=$(shell echo $(notdir $@) | sed 's/./\u&/')
	echo "[settings]\narch=x86_64\nos=$${CAP_PLATFORM}" > $@

${platforms}: ${CONAN_HOME}/profiles/default ${CONAN_HOME}/profiles/$$@
	@echo -e "\n*** Makefile: build: creating venv ***"
	if [ "$$VIRTUAL_ENV" == "" ] ; then
	    echo "creating python virtual environment in ./venv"
	    ${PYTHON} -m venv venv
	    source venv/bin/activate
	    pip install -r requirements.txt
	fi

	echo -e "\n*** Makefile: build: configuring conan ***"
	export CONAN_HOME="${CONAN_HOME}"  # copy from make env to bash env
	${CONAN} config install conan-config
	 #${CONAN} remote enable conancenter
	[ ! -e ".conan2/profiles/default" ] && ${CONAN} profile detect

	conan create --profile:build default --profile:host $@ --version ${VER} .

upload:
	@echo -e "\n*** Makefile: upload: activating venv ***"
	if [ "$$VIRTUAL_ENV" == "" ] ; then
	    echo -e "*** Makefile: upload: using python virtual environment in ./venv ***"
	    source venv/bin/activate
	fi

	echo -e "\n*** Makefile: upload: configuring conan ***"
	export CONAN_HOME="${CONAN_HOME}"  # copy from make env to bash env
	${CONAN} remote enable artifactory
	 # expects user in CONAN_LOGIN_USERNAME_ARTIFACTORY and password in CONAN_PASSWORD_ARTIFACTORY
	${CONAN} remote login artifactory

	conan remote add artifactory http://conan.jitx.com:8081/artifactory/api/conan/conan-local
	conan remote login artifactory jitx
	 # conan remove -r artifactory 'lbstanza/${VER}' # prompt for confirmation
	conan upload -r artifactory 'lbstanza/${VER}' -c
	 #
	 # NOTE: "export CONAN_PASSWORD_ARTIFACTORY=password"
	 #
.PHONY: upload

clean:
	rm -rf ${PWD}/.conan2*
.PHONY: clean
