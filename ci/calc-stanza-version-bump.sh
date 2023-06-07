#!/bin/bash
set -Eeuxo pipefail
PS4='>>> '
TOP="${PWD}"

# This script is designed to be run from a Concourse Task with the following env vars
# outputs the bumped version to stdout

# Required env var inputs
>&2 echo "            BRANCH:" "${BRANCH:?Usage: BRANCH=foo $0}"

# Defaulted env var inputs - can override if necessary
>&2 echo "           REPODIR:" "${REPODIR:=lbstanza}"
## By default, get the most recent previous tag on this branch from git
### note: if multiple tags exist on one commit, git doesn't always desecribe the most recent one
###    ### maybe use: git tag --sort=committerdate --contains HEAD~ | tail -1
###    ### also maybe git log --format=%H | git name-rev --stdin --name-only --tags
>&2 echo "           PREVTAG:" "${PREVTAG:=$(git -C "${REPODIR}" describe --tags --abbrev=0)}"
[[ "$PREVTAG" == "" ]] && PREVTAG=0.0.0-initial.0

## look for a change in the version number in compiler/params.stanza
## and if it changed in the most recent commit, use that as the version
### get the most recent commit, incuding merges
### look for changes in compiler/params.stanza
### if the version line was changed, extract the new version number
### and convert spaces to dots
>&2 echo "         PARAMSVER:" "${PARAMSVER:=$(git -C "${REPODIR}" log -1 -p -m  \
                                                | filterdiff -p1 -i compiler/params.stanza \
                                                | sed -E -n "s/^\+public val STANZA-VERSION = \[(.*)\].*/\1/p" \
                                                | head -1 | tr -s ', ' .
                                              )}"

if [[ "${PARAMSVER}" != "" ]] ; then
    # the version in compiler/params.stanza was changed, so use that as the version
    NEWVER=${PARAMSVER}
else
    ## By default, bump the prerelease version using the branch name
    ##   note the two dots after branch name are intentional
    BRANCH_OR_RC="${BRANCH}"
    if [[ "${BRANCH}" == "master" ]] ; then
        # if the branch is "master", use the prerelease label "rc"
        BRANCH_OR_RC="rc"
    fi
    >&2 echo "              BUMP:" "${BUMP:=prerel ${BRANCH_OR_RC}..}"

    ## if the previous tag is a final version with no prerel suffix
    ## then bump the patch version before adding a new suffix
    if [[ "${PREVTAG}" == "$(semver get release ${PREVTAG})" ]] ; then
        PREVTAG=$(semver bump patch ${PREVTAG})
    fi

    NEWVER=${PREVTAG}

    # keep bumping until the tag doesn't exist
    while [[ "${NEWVER}" == "${PREVTAG}" ]] || git -C "${REPODIR}" tag | grep -E -q "^${NEWVER}$" ; do
        ## Bump the previous tag
        BUMPEDTAG=$(semver bump ${BUMP} ${NEWVER})
        >&2 echo "         BUMPEDTAG:" "${BUMPEDTAG}"

        # calculate the new version tag by calling calc-stanza-version.sh in the same
        # directory as this script
        NEWVER=$($(dirname $0)/calc-stanza-version.sh "${BUMPEDTAG}" "${BRANCH}")
        >&2 echo "            NEWVER:" ${NEWVER}
    done
fi

echo ${NEWVER}
