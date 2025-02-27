# Lint as: python3
# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# pylint: disable=missing-docstring

import argparse
import os
import re
import subprocess
from typing import Sequence


def create_markdown_table(rows: Sequence[Sequence[str]]):
  """Converts a 2D array to a Markdown table."""
  return '\n'.join([' | '.join(row) for row in rows])


def check_and_get_output_lines(command: Sequence[str],
                               dry_run: bool = False,
                               log_stderr: bool = True):
  print(f'Running: `{" ".join(command)}`')
  if dry_run:
    return None, None
  process = subprocess.run(
      command,
      # TODO(#4131) python>=3.7: Use capture_output=True.
      stderr=subprocess.PIPE,
      stdout=subprocess.PIPE,
      # TODO(#4131) python>=3.7: Replace 'universal_newlines' with 'text'.
      universal_newlines=True)

  if log_stderr:
    for line in process.stderr.splitlines():
      print(line)

  process.check_returncode()

  return process.stdout.splitlines()


def get_test_targets(test_suite_path: str):
  """Returns a list of test targets for the given test suite."""
  # Check if the suite exists (which may not be true for failing suites).
  # We use two queries here because the return code for a failed query is
  # unfortunately the same as the return code for a bazel configuration error.
  target_dir = test_suite_path.split(':')[0]
  query = [
      'bazel', 'query', '--ui_event_filters=-DEBUG',
      '--noshow_loading_progress', '--noshow_progress', f'{target_dir}/...'
  ]
  targets = check_and_get_output_lines(query)
  if test_suite_path not in targets:
    return []

  query = [
      'bazel', 'query', '--ui_event_filters=-DEBUG',
      '--noshow_loading_progress', '--noshow_progress',
      f'tests({test_suite_path})'
  ]
  tests = check_and_get_output_lines(query)
  return tests
