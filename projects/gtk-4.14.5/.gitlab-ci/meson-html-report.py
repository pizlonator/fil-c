#!/usr/bin/env python3

# Turns a Mason testlog.json file into an HTML report
#
# Copyright 2019  GNOME Foundation
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Original author: Emmanuele Bassi

import argparse
import datetime
import json
import os
import sys
from jinja2 import Template

REPORT_TEMPLATE = '''
<!DOCTYPE html>
<html lang="en">
<head>
  <title>{{ report.project_name }} Test Report</title>
  <meta charset="utf-8" />
  <style type="text/css">
body {
  background: white;
  color: #333;
  font-family: 'Cantarell', sans-serif;
}

h1 {
  color: #333333;
  font-size: 1.9em;
  font-weight: normal;
  margin-bottom: 1em;
  border-bottom: 1px solid #333333;
}

header {
  position: fixed;
  padding-bottom: 12px;
  margin-bottom: 24px;
  background: rgba(255, 255, 255, 0.85);
  box-shadow: 0 0 1px rgba(0, 0, 0, 0.15);
  z-index: 500;
  left: 0;
  top: 0;
  width: 100%;
  color: rgba(0, 0, 0, 0.3);
  transform: translateY(0px);
  transition: .2s background-color, color;
  box-sizing: border-box;
  display: block;
  visibility: visible;
  text-align: center;
}

article {
  padding-top: 200px;
  margin: 2em;
}

div.report-meta {
  width: auto;
  border: 1px solid #ccc;
  padding: .5em 2em;
  color: #3c3c3c;
}

span.result {
  font-weight: bold;
}

span.pass {
  color: rgb(51, 209, 122);
}

span.skip {
  color: rgb(255, 163, 72);
}

span.fail {
  color: rgb(224, 27, 36);
}

span.xfail {
  color: rgb(163, 71, 186);
}

div.result {
  border-top: 1px solid #c0c0c0;
  padding-top: 1em;
  padding-bottom: 1em;
  width: 100%;
}

div.result h4 {
  border-bottom: 1px solid #c0c0c0;
  margin-bottom: 0.7em;
}

pre {
  color: #fafafa;
  background-color: black;
  border-radius: 6px;
  box-shadow: 0px 5px 8px 0px rgba(0, 0, 0, 0.25);
  font-family: monospace;
  line-height: 1.2em;
  border: none;
  padding: 10px 1em;
  font-size: 0.9em;
  overflow: auto;
  white-space: pre;
  word-break: normal;
  word-wrap: normal;
}

ul.passed li {
  display: inline;
}

ul.passed li:after {
  content: ",";
}

ul.passed li:last-child:after {
  content: "";
}

ul.images {
  padding-bottom: 1em;
}

ul.images li {
  display: inline;
}
  </style>
</head>
<body>
  <header>
    <h1>{{ report.project_name }}/{{ report.backend }}/{{ report.branch_name }} :: Test Reports</h1>
  </header>

  <article>
    <section>
      <div class="report-meta">
        <p><strong>Backend:</strong> {{ report.backend }}</p>
        <p><strong>Branch:</strong> {{ report.branch_name }}</p>
        <p><strong>Date:</strong> <time datetime="{{ report.date.isoformat() }}">{{ report.locale_date }}</time></p>
        {% if report.job_id %}<p><strong>Job ID:</strong> {{ report.job_id }}</p>{% endif %}
      </div>
    </section>

    <section>
      <div class="summary">
        <h3><a name="summary">Summary</a></h3>
        <ul>
          <li><strong>Total units:</strong> {{ report.total_units }}</li>
          <li><strong>Failed:</strong> {{ report.total_failures }}</li>
          <li><strong>Passed:</strong> {{ report.total_successes }}</li>
        </ul>
      </div>
    </section>

    {% for suite_result in report.results_list %}
    <section>
      <div class="result">
        <h3><a name="results">Suite: {{ suite_result.suite_name }}</a></h3>
        <ul>
          <li><strong>Units:</strong> {{ suite_result.n_units }}</li>
          <li><strong>Failed:</strong> <a href="#{{ suite_result.suite_name }}-failed">{{ suite_result.n_failures }}</a></li>
          <li><strong>Passed:</strong> <a href="#{{ suite_result.suite_name }}-passed">{{ suite_result.n_successes }}</a></li>
        </ul>

        <div class="failures">
          <h4><a name="{{ suite_result.suite_name }}-failed">Failures</a></h4>
          <ul class="failed">
            {% for failure in suite_result.failures if failure.result in [ 'ERROR', 'FAIL', 'UNEXPECTEDPASS' ] %}
            <li><a name="{{ failure.name }}">{{ failure.name }}</a> - result: <span class="result fail">{{ failure.result }}</span><br/>
              {% if failure.stdout %}
              Output: <pre>{{ failure.stdout }}</pre>
              {% endif %}
              {% if failure.image_data is defined %}
              <ul class="images">
                <li><img alt="ref" src="{{ failure.image_data.ref }}" /></li>
                <li><img alt="out" src="{{ failure.image_data.out }}" /></li>
                <li><img alt="diff" src="{{ failure.image_data.diff }}" /></li>
                <li><a href="{{ failure.image_data.refnode }}">ref node</a></li>
                <li><a href="{{ failure.image_data.outnode }}">out node</a></li>
              </ul>
              {% endif %}
            </li>
            {% else %}
            <li>None</li>
            {% endfor %}
          </ul>

          <h4><a name="{{ suite_result.suite_name }}-timed-out">Timed out</a></h4>
          <ul class="failed">
            {% for failure in suite_result.failures if failure.result == 'TIMEOUT' %}
            <li><a name="{{ failure.name }}">{{ failure.name }}</a> - result: <span class="result fail">{{ failure.result }}</span><br/>
              {% if failure.stdout %}
              Output: <pre>{{ failure.stdout }}</pre>
              {% endif %}
            </li>
            {% else %}
            <li>None</li>
            {% endfor %}
          </ul>
        </div>

        <div class="successes">
          <h4><a name="{{ suite_result.suite_name }}-expected-fail">Expected failures</a></h4>
          <ul>
          {% for success in suite_result.successes if success.result == 'EXPECTEDFAIL' %}
            <li><a name="{{ success.name }}">{{ success.name }}</a> - result: <span class="result xfail">{{ success.result }}</span><br/>
            {% if success.stdout %}
              Output: <pre>{{ success.stdout }}</pre>
            {% endif %}
            {% if success.image_data is defined %}
              <ul class="images">
                <li><img alt="ref" src="{{ success.image_data.ref }}" /></li>
                <li><img alt="out" src="{{ success.image_data.out }}" /></li>
                <li><img alt="diff" src="{{ success.image_data.diff }}" /></li>
              </ul>
            {% endif %}
            </li>
          {% else %}
            <li>None</li>
          {% endfor %}
          </ul>

          <h4><a name="{{ suite_result.suite_name }}-skipped">Skipped</a></h4>
          <ul>
            {% for success in suite_result.successes if success.result == 'SKIP' %}
            <li>{{ success.name }} - result: <span class="result skip">{{ success.result }}</li>
            {% else %}
            <li>None</li>
            {% endfor %}
          </ul>

          <h4><a name="{{ suite_result.suite_name }}-passed">Passed</a></h4>
          <ul class="passed">
            {% for success in suite_result.successes if success.result == 'OK' %}
            <li>{{ success.name }} - result: <span class="result pass">{{ success.result }}</li>
            {% else %}
            <li>None</li>
            {% endfor %}
          </ul>
        </div>

      </div>
    </section>
    {% endfor %}

  </article>
</body>
</html>
'''

aparser = argparse.ArgumentParser(description='Turns a Meson test log into an HTML report')
aparser.add_argument('--project-name', metavar='NAME',
                     help='The project name',
                     default='Unknown')
aparser.add_argument('--backend', metavar='NAME',
                     help='The used backend',
                     default='unknown')
aparser.add_argument('--job-id', metavar='ID',
                     help='The job ID for the report',
                     default=None)
aparser.add_argument('--branch', metavar='NAME',
                     help='Branch of the project being tested',
                     default='main')
aparser.add_argument('--output', metavar='FILE',
                     help='The output HTML file, stdout by default',
                     type=argparse.FileType('w', encoding='UTF-8'),
                     default=sys.stdout)
aparser.add_argument('--reftest-suite', metavar='NAME',
                     help='The name of the reftests suite',
                     default='reftest')
aparser.add_argument('--reftest-output-dir', metavar='DIR',
                     help='The output directory for reftests data',
                     default=None)
aparser.add_argument('infile', metavar='FILE',
                     help='The input testlog.json, stdin by default',
                     type=argparse.FileType('r', encoding='UTF-8'),
                     default=sys.stdin)

args = aparser.parse_args()

outfile = args.output

suites = {}
for line in args.infile:
    data = json.loads(line)
    (full_suite, unit_name) = data['name'].split(' / ')
    (project_name, suite_name) = full_suite.split(':')

    unit = {
        'project-name': project_name,
        'suite': suite_name,
        'name': unit_name,
        'duration': data['duration'],
        'returncode': data['returncode'],
        'result': data['result'],
        'stdout': data['stdout'],
    }

    if args.reftest_output_dir is not None and suite_name == args.reftest_suite:
        filename = unit_name.split(' ')[1]
        basename = os.path.splitext(filename)[0]

        image_data = {
            'ref': os.path.join(args.reftest_output_dir, '{}.ref.png'.format(basename)),
            'out': os.path.join(args.reftest_output_dir, '{}.out.png'.format(basename)),
            'refnode': os.path.join(args.reftest_output_dir, '{}.ref.node'.format(basename)),
            'outnode': os.path.join(args.reftest_output_dir, '{}.out.node'.format(basename)),
            'diff': os.path.join(args.reftest_output_dir, '{}.diff.png'.format(basename)),
        }

        unit['image_data'] = image_data

    units = suites.setdefault(full_suite, [])
    units.append(unit)

report = {}
report['date'] = datetime.datetime.now(datetime.UTC)
report['locale_date'] = report['date'].strftime("%c")
report['project_name'] = args.project_name
report['backend'] = args.backend
report['job_id'] = args.job_id
report['branch_name'] = args.branch
report['total_successes'] = 0
report['total_failures'] = 0
report['total_units'] = 0
report['results_list'] = []

for name, units in suites.items():
    (project_name, suite_name) = name.split(':')
    print('Processing {} suite {}:'.format(project_name, suite_name))

    def if_failed(unit):
        if unit['result'] in ['FAIL', 'UNEXPECTEDPASS', 'TIMEOUT', 'ERROR',]:
            return True
        return False

    def if_succeded(unit):
        if unit['result'] in ['OK', 'EXPECTEDFAIL', 'SKIP']:
            return True
        return False

    successes = list(filter(if_succeded, units))
    failures = list(filter(if_failed, units))

    n_units = len(units)
    n_successes = len(successes)
    n_failures = len(failures)

    report['total_units'] += n_units
    report['total_successes'] += n_successes
    report['total_failures'] += n_failures
    print(' - {}: {} total, {} pass, {} fail'.format(suite_name, n_units, n_successes, n_failures))

    suite_report = {
        'suite_name': suite_name,
        'n_units': n_units,
        'successes': successes,
        'n_successes': n_successes,
        'failures': failures,
        'n_failures': n_failures,
    }
    report['results_list'].append(suite_report)

template = Template(REPORT_TEMPLATE)
outfile.write(template.render({'report': report}))
