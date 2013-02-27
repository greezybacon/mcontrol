#!/bin/env python

from mcontrol.cli import Shell, parser

if __name__ == '__main__':
    options, args = parser.parse_args()
    Shell(options=options, scripts=args).run()
