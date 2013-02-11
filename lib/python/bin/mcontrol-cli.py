#!/bin/env python

from mcontrol.cli import cli

if __name__ == '__main__':
    options, args = cli.parser.parse_args()
    cli.Shell(options=options, scripts=args).run()
