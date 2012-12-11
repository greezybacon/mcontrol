from pyPEG import keyword, ignore
import re

# Bare -- commands that (can) take no arguments
bare = {'E', 'S', 'H', 'RT'}
# Commands -- called in the form of COMMAND <space> ARG, ARG
commands = {'CL', 'DC', 'IC', 'H', 'HM', 'MA', 'MR', 'OE', 'PR', 'SL', 'TI',
    'BR'}
# Read-only -- assignment is not allowed
read_only = {'BY', 'EF', 'I1', 'I2', 'I3', 'I4', 'I5', 'I6', 'IF', 'IH',
    'IL', 'IN', 'IT', 'MV', 'PC', 'PN', 'SN', 'V', 'VC', 'VR'}
# Read-write -- assignment in the form of VAR = VAL, VAL2
read_write = {'A', 'C1', 'C2', 'CM', 'CR', 'D', 'D1', 'D2', 'D3', 'D4',
    'D5', 'DB', 'DE', 'EE', 'ES', 'ER', 'FC', 'FM', 'HC', 'HT', 'JE', 'LM',
    'MS', 'MT', 'NE', 'P', 'PM', 'R1', 'R2', 'R3', 'R4', 'RC', 'S1', 'S2',
    'S3', 'S4', 'S5', 'SF', 'SM', 'ST', 'TE', 'VI', 'VM'}

internal = commands | bare | read_only | read_write
assignable = commands | read_write

def comment():      return re.compile("[ \t]*'.*$", re.M)

def declaration():  return keyword("VA"), _ws, name, \
                           0, (_ws, "=", _ws, expression)
def name():         return re.compile(r'[A-Za-z]\w?')
def assignment():   return name, _ws, 0, "=", \
                           _ws, expression, -1, (_ws, ",", _ws, atom)
def literal():      return [number, string]
def number():       return re.compile(r'-?\d+')    # No floating point math
def string():       return re.compile(r'"[^"]*"')
def configvar():    return "$", re.compile(r'[\w_.]+')
def label():        return keyword("LB"), _ws1, name
def branch():       return keyword("BR"), _ws1, name, _ws, 0, (",", _ws, expression)
def call():         return keyword("CL"), _ws1, name, _ws, 0, (",", _ws, expression)
def return_():      return keyword("RT")
def command():      return re.compile('|'.join(bare))
def atom():         return [literal, name, configvar]
def expression():   return atom, _ws, 0, ([compare, math], _ws, expression)
def compare():      return re.compile(r"<>|<=|>=|<|>|=")
def math():         return re.compile(r"[+*/&\|!^-]")

def statement():    return [call, branch, return_, label,
                            declaration, assignment, command], 0, comment, _newline

def _ws1():         return ignore('[ \t]+')
def _ws():          return ignore('[ \t]*')
def _newline():     return ignore('\n|\r\n')

def pragma():       return ignore("#"), _ws, re.compile(".*$", re.M), _newline

def language():     return -2, (_ws, [pragma, statement,
                            (comment, _newline), _newline])
