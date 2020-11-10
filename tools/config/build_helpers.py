#! /usr/bin/env python3
# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

"""build_helpers.py : Contains classes and helper funcs for build automation """

__author__ = "AMD Research"
__copyright__   = "Copyright 2019"

import os
import sys
import shutil
import subprocess
import psutil
import jsonpickle
from collections import MutableMapping
from git_helpers import *

BUILD_CMD = 'cmake'

BUILD_TYPE = 'RelWithDebInfo'


def clean_dir(dir_path):
    """ remove contents of a directory """
    for root, dirs, files in os.walk(dir_path):
        if files:
            for _f in files:
                os.unlink(os.path.join(root, _f))
        if dirs:
            for _d in dirs:
                shutil.rmtree(os.path.join(root, _d), ignore_errors=True)

def check_make_clean_dir(dir_path, clean=False):
    """ Create a new directory path, clean it if it exists """
    if (os.path.exists(dir_path)):
        if clean:
            clean_dir(dir_path)
    else:
        os.makedirs(dir_path)

def copy_dir(src, dest):
    """ Copy a source directory """
    if (os.path.exists(src) and os.path.isdir(src)):
        try:
            shutil.copytree(src, dest, Symlinks=True)
        except OSError as e:
            print(f'Copy Dir failed with error: {e}')
                # the provided address is a file

def check_obj(obj):
    """ Check if object exists """
    if obj:
        return True
    else:
        return False

class Project(MutableMapping):
    """
    Project Datastruct
    """
    CMAKE_DICT = {'CMAKE_BUILD_TYPE' : BUILD_TYPE}
    ROOT_DICT = {}

    def __init__(self, data=None):
        self._items = {}
        self.update(self.ROOT_DICT)
        if data:
            self.update(data)
    
    def __getitem__(self, key):
        try:
            return self._items[key]
        except KeyError:
            raise
    
    def __setitem__(self, key, value):
        self._items[key] = value
        
    def __delitem__(self):
        try:
            del self._items[key]
        except KeyError:
            raise

    def __repr__(self):
        return repr(self._items)

    def __str__(self):
        return str(self._items)

    def __iter__(self):
        return iter(self._items)
    
    def __len__(self):
        return len(self._items)

    def to_json(self, outfile=None):
        """ Convert the Object into a json for storing future state """
        jsonpickle.set_encoder_options('simplejson', sort_keys=False, indent=2)
        json_repr = jsonpickle.encode(self._items)
        if not outfile is None:
            with open(outfile, 'w+') as f:
                f.write(json_repr)
        return json_repr

    def from_json(self, infile):
        """ Read json file into Project object """
        ret_obj = None
        with open(infile) as f:
            ret_obj = jsonpickle.decode(f.read())
        return ret_obj

    def fetch(self, clean=False):
        """Clone the repo or copy sources to the working folder"""
        #TODO: add fetch check logic as wraper to check_make_clean_dir
        # If fetch config in folder then return true and don't re-fetch 
        if 'fetched' in self._items.keys():
            if self._items['fetched'] and not clean:
                return True
        if self._items['type'] is 'git':
            path = os.path.join( self._items['root_dir'],
                                 self._items['name'],
                                 'src')
            check_make_clean_dir(path, clean=True)
            repo = clone_repo( self._items['address'], 
                               path, 
                               self._items['branch_or_commit'])
            self._items['commit'] = get_commit_id(path)
            
            if 'src_path' in self._items.keys():
                self._items['src_path'] = os.path.join(path, self._items['src_path'])
            else:
                self._items['src_path'] = path
                
            self._items['fetched'] = True
            self._items['repo_obj'] = repo
            self.to_json(outfile=os.path.join(path, f"{self._items['name']}.fetch.config"))
            return True
        
        elif self._items['type'] is 'dir':
            path = os.path.join( self._items['root_dir'],
                                 self._items['name'],
                                 'src')
            check_make_clean_dir(path, clean=True)
            # Copy a working directory to the src folder
            copy_dir(_items['address'], path)
            if 'src_path' in self._items.keys():
                self._items['src_path'] = os.path.join(path, self._items['src_path'])
            else:
                self._items['src_path'] = path
            self._items['fetched'] = True
            return True
        else:
            print("unknown project type")
            return False

    def build_run_cmake(self):
        """ Run CMake to generate the makefiles in the build dir """
        # TODO: Take dependencies into account
        if self._items['build']:
            # Create build directory
            build_path = os.path.join( self._items['root_dir'],
                                 self._items['name'],
                                 'build',
                                 self._items['commit'])
            check_make_clean_dir(build_path)
            # update env flags
            global_env = os.environ
            flags = self._items['flags']

            if 'env' in flags.keys():
                for env, value in flags['env'].items():
                    oldval = global_env.get(env)
                    if not (oldval is None):
                        global_env[env] = oldval+':'+value
                    else:
                        global_env[env] = value
                    print(global_env[env])
            
            BUILD_FLAGS = [f'-D{i}={v}' for i,v in self.CMAKE_DICT.items()]
            if 'build' in flags.keys():
                BUILD_FLAGS += [f'-D{i}={v}' for i,v in flags['build'].items()]
            
            # create build command
            cmd = " ".join([BUILD_CMD] + BUILD_FLAGS + [self._items['src_path']])
            print(cmd)
            print(build_path)
            # Do cmake config build action
            with open(os.path.join(build_path, self._items['name']+".build.log"), "a") as f:
                cmake_sucess = False
                _a = None
                try:
                    if sys.version_info > (3,7):
                        _a = subprocess.run(cmd,
                                        cwd=build_path,
                                        env=global_env,
                                        check=True, 
                                        shell=True,
                                        text=True, 
                                        capture_output=True)
                    else:
                         _a = subprocess.run(cmd,
                                        cwd=build_path,
                                        env=global_env,
                                        check=True, 
                                        shell=True,
                                        universal_newlines=True,
                                        stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE)
                    cmake_sucess = True
                except subprocess.CalledProcessError as e:
                    _a = e
                    cmake_sucess = False
                    print("ERROR: Build Failed") # fix print to stderr
                f.write("CMAKE BUILD\nSTDOUT:\n")
                f.write(_a.stdout)
                f.write("\nERROR:\n")
                f.write(_a.stderr)
                                
                # if cmake build was sucessfull go run make in the build directory
                if cmake_sucess:
                    make_sucess = False
                    make_cmd = f'make -j {psutil.cpu_count(logical=False)}'
                    try:
                        if sys.version_info > (3,7):
                            _a = subprocess.run(make_cmd,
                                            cwd=build_path,
                                            env=global_env,
                                            check=True, 
                                            shell=True, 
                                            text=True, 
                                            capture_output=True)
                        else:
                            _a = subprocess.run(make_cmd,
                                            cwd=build_path,
                                            env=global_env,
                                            check=True, 
                                            shell=True, 
                                            universal_newlines=True,
                                            stdout=subprocess.PIPE,
                                            stderr=subprocess.PIPE)
                        self._items['build_path'] = build_path
                        make_sucess = True
                    except subprocess.CalledProcessError as e:
                        _a = e
                        print(f"ERROR: Build Failed with error {e}") # fix print to stderr
                    f.write("\nRUNNING MAKE\nSTDOUT:\n")
                    f.write(_a.stdout)
                    f.write("\nERROR:\n")
                    f.write(_a.stderr)

                    # Write the build config on success
                    if not make_sucess:
                        return False
                    self.to_json(outfile=os.path.join(build_path, f"{self._items['name']}.build.config"))
                    return True

                else:
                    return False

        else:
            print(f"{self._items['name']} not required to built")
            return True
    
    def build(self, sys='cmake'):
        """ Call on the correct build system helper 
            For now cmake is the default and only supported system 
        """
        if self._items['build']:
            if sys is 'cmake':
                return self.build_run_cmake()
        else:
            print(f"Not required to build {self._items['name']}")
            return True
        return False
        
    def check_dependencies(self):
        """ Resolve the dependencies to update specified fields in Project """
        if not 'deps' in self._items.keys():
            print(f"No dependencies specified for {self._items['name']}")
            return True
        for name, dependency, fields in self._items['deps']:
            print(name, dependency, fields)
            if not check_obj(dependency):
                # dependency object not defined
                print(f"ERROR: Object for dependency {name} not defined")
                return False
            # Assuming the object exists lets fetch and build them
            if dependency.fetch():
                if not dependency.build():
                    print(f'ERROR: {name} build failed')
                    return False
                # fill in the flags
                for flag, value in fields.items():
                    keys = flag.split('.')
                    # print(keys, len(keys))
                    if len(keys) == 1:
                        # Assuming this is a root level key
                        if keys[0] in self._items.keys():
                            # override existing value
                            self._items[keys[0]] = value
                    elif len(keys) == 2:
                        # this is the only valid way
                        if keys[0] == 'flags':
                            # print(keys, value)
                            set_value = None
                            if keys[1] == 'build':
                                if dependency.get('build'):
                                    set_value = dependency.get('build_path')
                                else:
                                    set_value = dependency.get('src_path')
                                self._items[keys[0]][keys[1]][value] = set_value
                                # print( self._items, set_value)
                            if keys[1] == 'env':
                                if value is 'LD_LIBRARY_PATH':
                                    # this is a library hook, satisfied only
                                    # for built libraries, ignore for others
                                    if dependency.get('build'):
                                        set_value = os.path.join(dependency.get('build_path'),
                                                                 'lib')
                                        if value in self._items[keys[0]][keys[1]].keys():
                                            old_val = self._items[keys[0]][keys[1]].get(value)
                                            set_value = set_value +':'+ old_val
                                        self._items[keys[0]][keys[1]][value] = set_value
                                else:
                                    if dependency.get('build'):
                                        set_value = dependency.get('build_path')
                                    else:
                                        set_value = dependency.get('src_path')
                                    self._items[keys[0]][keys[1]][value] = set_value
                    else:
                        print("ERROR: Invalid input")
                        return False
            else:
                print(f"{name} fetch failed")
                return False
        # end for
        return True


