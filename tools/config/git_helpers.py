#! /usr/bin/env python3
# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.
#
# Parts of this code were adapted from https://www.fullstackpython.com/blog/first-steps-gitpython.html
# available under MIT License: https://github.com/mattmakai/fullstackpython.com/blob/master/LICENSE
# Copyright (c) 2017 Matthew Makai

"""git_helpers.py : Contains classes and helper funcs for build automation """

__author__ = "AMD Research"
__copyright__   = "Copyright 2019"


import os
from git import Repo

def print_repo(repo):
    """ 
    Prints basic repo info for debugging
    Adapted from https://www.fullstackpython.com/blog/first-steps-gitpython.html
    """
    print(f'Repo description: {repo.description}')
    # print(f'Repo active branch is {repo.active_branch}')
    for remote in repo.remotes:
        print(f'Remote named "{remote}" with URL "{remote.url}"')
    print(f'Last commit for repo is {repo.head.commit.hexsha}.')
    
def print_commit(commit):
    """
    Prints commit info for debugging
    Adapted from https://www.fullstackpython.com/blog/first-steps-gitpython.html
    """
    print('----')
    print(str(commit.hexsha))
    print(f"\"{commit.summary}\" by {commit.author.name} ({commit.author.email})")
    print(commit.authored_datetime)
    print(f"count: {commit.count()} and size: {commit.size}")
    
def checkout_commit(commit, repo_dir):
    """ Takes repo path and checks out Commit Hex SHA """
    repo = Repo(repo_dir)
    if not repo.bare:
        print(f'Repo at {repo_dir} successfully loaded.')
        print_repo(repo)
        repo.git.checkout(commit, '--recurse-submodules')
    else :
         print(f'Could not load repository at {repo_dir}')
    return repo

def list_commits_for_branch(branch, repo_dir):
    """returns list of commits sorted in most recent first order"""
    repo = Repo(repo_dir)
    if not repo.bare:
        print(f'Repo at {repo_dir} successfully loaded.')
        print_repo(repo)
        # check if branch exists
        if branch in [str(i) for i in repo.branches]:
            print(f'Unable to find {branch} in repo')
            return None
        #return list of commits from branch:
        commits = list(repo.iter_commits(branch))
        for commit in commits:
            print_commit(commit)
            pass
        return commits
    else :
        print(f'Could not load repository at {repo_dir}')
        return None

def reset_to_commit(commit, repo_dir):
    """ Take a commit object and reset repo to it """
    repo = Repo(repo_dir)
    if not repo.bare:
        print(f'Repo at {repo_dir} successfully loaded.')
        print_repo(repo)
        repo.head.reset(commit=commit.hexsha, index=True, working_tree=True)
    else :
         print(f'Could not load repository at {repo_dir}')
    return repo

def clone_repo(repo_addr, clone_in_dir, branch_or_commit):
    """ Clone repo into specified directory and checkout to specific branch/commit """
    repo = Repo.clone_from(repo_addr, clone_in_dir)
    print_repo(repo)
    if not repo.bare:
        print(f'Checking out Repo {repo_addr} to {branch_or_commit}')
        repo.git.checkout(branch_or_commit, "--recurse-submodules")
        get_submodules(repo)
        print_repo(repo)
    else:
        print(f'Bare repo {repo_addr}, unable to checkout')
    return repo

def get_commit_id(repo_dir):
    """ Get commit for the listed repo directory """
    repo = Repo(repo_dir)
    if not repo.bare:
        print(f'Repo at {repo_dir} successfully loaded.')
        print_repo(repo)
        return repo.head.commit.hexsha
    else:
        print(f'Could not load repository at {repo_dir}')
    return None 

def get_submodules(repo):
    """ Get submodules for a repo """
    if not repo.bare:
        print("Fetching Git submodules")
        for submodule in repo.submodules:
            submodule.update(init=True)
