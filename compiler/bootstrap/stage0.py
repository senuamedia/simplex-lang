#!/usr/bin/env python3
"""
Stage 0 Simplex Compiler - Written in Python to bootstrap the minimal compiler
This compiles a subset of Simplex to LLVM IR without depending on Simplex itself.
Rewritten to avoid dataclasses for Python 3.14 compatibility.

Copyright (c) 2025-2026 Rod Higgins
Licensed under MIT License - see LICENSE file
https://github.com/senuamedia/simplex-lang
"""

import sys
import struct
import platform

def get_target_triple():
    """Get the LLVM target triple for the current platform."""
    system = platform.system().lower()
    machine = platform.machine().lower()

    # Normalize architecture
    if machine in ('x86_64', 'amd64'):
        arch = 'x86_64'
    elif machine in ('arm64', 'aarch64'):
        arch = 'aarch64'
    elif machine in ('i386', 'i686'):
        arch = 'i386'
    else:
        arch = 'x86_64'  # Default

    # Build target triple based on OS
    if system == 'darwin':
        # macOS
        version = platform.mac_ver()[0]
        if version:
            major = version.split('.')[0]
            return f'{arch}-apple-macosx{major}.0.0'
        return f'{arch}-apple-macosx14.0.0'
    elif system == 'windows':
        return f'{arch}-pc-windows-msvc'
    else:
        # Linux and other Unix-like systems
        return f'{arch}-unknown-linux-gnu'

# Token types
class TokenKind:
    EOF = 'EOF'
    IDENT = 'IDENT'
    INT = 'INT'
    FLOAT = 'FLOAT'
    STRING = 'STRING'
    FSTRING = 'FSTRING'  # f"..." interpolated string
    LPAREN = 'LPAREN'
    RPAREN = 'RPAREN'
    LBRACE = 'LBRACE'
    RBRACE = 'RBRACE'
    LBRACKET = 'LBRACKET'
    RBRACKET = 'RBRACKET'
    COMMA = 'COMMA'
    COLON = 'COLON'
    SEMI = 'SEMI'
    ARROW = 'ARROW'
    DOUBLE_COLON = 'DOUBLE_COLON'
    EQ = 'EQ'
    EQEQ = 'EQEQ'
    NE = 'NE'
    LT = 'LT'
    GT = 'GT'
    LE = 'LE'
    GE = 'GE'
    PLUS = 'PLUS'
    MINUS = 'MINUS'
    STAR = 'STAR'
    SLASH = 'SLASH'
    PERCENT = 'PERCENT'
    BANG = 'BANG'
    DOT = 'DOT'
    DOTDOT = 'DOTDOT'
    AMP = 'AMP'
    AMPAMP = 'AMPAMP'
    PIPE = 'PIPE'
    PIPEPIPE = 'PIPEPIPE'
    CARET = 'CARET'
    LTLT = 'LTLT'
    GTGT = 'GTGT'
    # Keywords
    KW_FN = 'KW_FN'
    KW_LET = 'KW_LET'
    KW_IF = 'KW_IF'
    KW_ELSE = 'KW_ELSE'
    KW_WHILE = 'KW_WHILE'
    KW_FOR = 'KW_FOR'
    KW_IN = 'KW_IN'
    KW_RETURN = 'KW_RETURN'
    KW_ENUM = 'KW_ENUM'
    KW_TRUE = 'KW_TRUE'
    KW_FALSE = 'KW_FALSE'
    KW_PUB = 'KW_PUB'
    KW_STRUCT = 'KW_STRUCT'
    KW_IMPL = 'KW_IMPL'
    KW_SELF = 'KW_SELF'
    KW_MATCH = 'KW_MATCH'
    KW_BREAK = 'KW_BREAK'
    KW_CONTINUE = 'KW_CONTINUE'
    KW_VAR = 'KW_VAR'
    KW_MODULE = 'KW_MODULE'
    KW_MOD = 'KW_MOD'
    KW_USE = 'KW_USE'
    # Actor model keywords
    KW_ACTOR = 'KW_ACTOR'
    KW_RECEIVE = 'KW_RECEIVE'
    KW_SPAWN = 'KW_SPAWN'
    KW_SEND = 'KW_SEND'
    KW_ASK = 'KW_ASK'
    KW_INIT = 'KW_INIT'
    # AI/Hive keywords
    KW_SPECIALIST = 'KW_SPECIALIST'
    KW_HIVE = 'KW_HIVE'
    KW_INFER = 'KW_INFER'
    KW_AWAIT = 'KW_AWAIT'
    KW_YIELD = 'KW_YIELD'
    # Advanced features
    KW_ASYNC = 'KW_ASYNC'
    KW_TRAIT = 'KW_TRAIT'
    KW_TYPE = 'KW_TYPE'
    KW_CONST = 'KW_CONST'
    KW_WHERE = 'KW_WHERE'
    KW_MUT = 'KW_MUT'
    QUESTION = 'QUESTION'
    FAT_ARROW = 'FAT_ARROW'
    UNDERSCORE = 'UNDERSCORE'
    HASH = 'HASH'

KEYWORDS = {
    'fn': TokenKind.KW_FN,
    'let': TokenKind.KW_LET,
    'if': TokenKind.KW_IF,
    'else': TokenKind.KW_ELSE,
    'while': TokenKind.KW_WHILE,
    'for': TokenKind.KW_FOR,
    'in': TokenKind.KW_IN,
    'return': TokenKind.KW_RETURN,
    'enum': TokenKind.KW_ENUM,
    'true': TokenKind.KW_TRUE,
    'false': TokenKind.KW_FALSE,
    'pub': TokenKind.KW_PUB,
    'struct': TokenKind.KW_STRUCT,
    'impl': TokenKind.KW_IMPL,
    'self': TokenKind.KW_SELF,
    'Self': TokenKind.KW_SELF,  # Both self and Self map to same token
    'match': TokenKind.KW_MATCH,
    'break': TokenKind.KW_BREAK,
    'continue': TokenKind.KW_CONTINUE,
    'var': TokenKind.KW_VAR,
    'module': TokenKind.KW_MODULE,
    'mod': TokenKind.KW_MOD,
    'use': TokenKind.KW_USE,
    'actor': TokenKind.KW_ACTOR,
    'receive': TokenKind.KW_RECEIVE,
    'spawn': TokenKind.KW_SPAWN,
    'send': TokenKind.KW_SEND,
    'ask': TokenKind.KW_ASK,
    'init': TokenKind.KW_INIT,
    'specialist': TokenKind.KW_SPECIALIST,
    'hive': TokenKind.KW_HIVE,
    'infer': TokenKind.KW_INFER,
    'await': TokenKind.KW_AWAIT,
    'yield': TokenKind.KW_YIELD,
    'async': TokenKind.KW_ASYNC,
    'trait': TokenKind.KW_TRAIT,
    'type': TokenKind.KW_TYPE,
    'const': TokenKind.KW_CONST,
    'where': TokenKind.KW_WHERE,
    'mut': TokenKind.KW_MUT,
    '_': TokenKind.UNDERSCORE,
}

class Token:
    def __init__(self, kind, text, pos):
        self.kind = kind
        self.text = text
        self.pos = pos

class Lexer:
    def __init__(self, source):
        self.source = source
        self.pos = 0

    def at_end(self):
        return self.pos >= len(self.source)

    def peek(self):
        if self.at_end():
            return '\0'
        return self.source[self.pos]

    def peek_next(self):
        if self.pos + 1 >= len(self.source):
            return '\0'
        return self.source[self.pos + 1]

    def advance(self):
        if self.at_end():
            return '\0'
        c = self.source[self.pos]
        self.pos += 1
        return c

    def skip_whitespace(self):
        while self.peek() in ' \t\n\r':
            self.advance()

    def skip_line_comment(self):
        while not self.at_end() and self.peek() != '\n':
            self.advance()

    def next_token(self):
        self.skip_whitespace()

        if self.at_end():
            return Token(TokenKind.EOF, '', self.pos)

        start = self.pos
        c = self.advance()

        # Single char tokens
        if c == '(': return Token(TokenKind.LPAREN, c, start)
        if c == ')': return Token(TokenKind.RPAREN, c, start)
        if c == '{': return Token(TokenKind.LBRACE, c, start)
        if c == '}': return Token(TokenKind.RBRACE, c, start)
        if c == '[': return Token(TokenKind.LBRACKET, c, start)
        if c == ']': return Token(TokenKind.RBRACKET, c, start)
        if c == ',': return Token(TokenKind.COMMA, c, start)
        if c == ';': return Token(TokenKind.SEMI, c, start)
        if c == '+': return Token(TokenKind.PLUS, c, start)
        if c == '*': return Token(TokenKind.STAR, c, start)
        if c == '%': return Token(TokenKind.PERCENT, c, start)
        if c == '#': return Token(TokenKind.HASH, c, start)
        if c == '.':
            if self.peek() == '.':
                self.advance()
                return Token(TokenKind.DOTDOT, '..', start)
            return Token(TokenKind.DOT, c, start)

        # Two char tokens
        if c == ':':
            if self.peek() == ':':
                self.advance()
                return Token(TokenKind.DOUBLE_COLON, '::', start)
            return Token(TokenKind.COLON, c, start)

        if c == '-':
            if self.peek() == '>':
                self.advance()
                return Token(TokenKind.ARROW, '->', start)
            return Token(TokenKind.MINUS, c, start)

        if c == '=':
            if self.peek() == '=':
                self.advance()
                return Token(TokenKind.EQEQ, '==', start)
            if self.peek() == '>':
                self.advance()
                return Token(TokenKind.FAT_ARROW, '=>', start)
            return Token(TokenKind.EQ, c, start)

        if c == '!':
            if self.peek() == '=':
                self.advance()
                return Token(TokenKind.NE, '!=', start)
            return Token(TokenKind.BANG, c, start)

        if c == '<':
            if self.peek() == '=':
                self.advance()
                return Token(TokenKind.LE, '<=', start)
            if self.peek() == '<':
                self.advance()
                return Token(TokenKind.LTLT, '<<', start)
            return Token(TokenKind.LT, c, start)

        if c == '>':
            if self.peek() == '=':
                self.advance()
                return Token(TokenKind.GE, '>=', start)
            if self.peek() == '>':
                self.advance()
                return Token(TokenKind.GTGT, '>>', start)
            return Token(TokenKind.GT, c, start)

        if c == '&':
            if self.peek() == '&':
                self.advance()
                return Token(TokenKind.AMPAMP, '&&', start)
            return Token(TokenKind.AMP, c, start)

        if c == '?':
            return Token(TokenKind.QUESTION, c, start)

        if c == '|':
            if self.peek() == '|':
                self.advance()
                return Token(TokenKind.PIPEPIPE, '||', start)
            return Token(TokenKind.PIPE, c, start)

        if c == '^':
            return Token(TokenKind.CARET, c, start)

        if c == '/':
            if self.peek() == '/':
                self.skip_line_comment()
                return self.next_token()
            return Token(TokenKind.SLASH, c, start)

        # f-string literals: f"..."
        if c == 'f' and self.peek() == '"':
            self.advance()  # consume opening quote
            text = ''
            while not self.at_end() and self.peek() != '"':
                ch = self.advance()
                if ch == '\\' and not self.at_end():
                    esc = self.advance()
                    if esc == 'n':
                        text += '\n'
                    elif esc == 't':
                        text += '\t'
                    elif esc == '"':
                        text += '"'
                    elif esc == '\\':
                        text += '\\'
                    elif esc == '{':
                        text += '{'
                    elif esc == '}':
                        text += '}'
                    else:
                        text += esc
                else:
                    text += ch
            self.advance()  # consume closing quote
            return Token(TokenKind.FSTRING, text, start)

        # Identifiers and keywords
        if c.isalpha() or c == '_':
            while self.peek().isalnum() or self.peek() == '_':
                c += self.advance()
            kind = KEYWORDS.get(c, TokenKind.IDENT)
            return Token(kind, c, start)

        # Numbers (integers and floats)
        if c.isdigit():
            while self.peek().isdigit():
                c += self.advance()
            # Check for floating-point
            if self.peek() == '.' and self.peek_next().isdigit():
                c += self.advance()  # consume '.'
                while self.peek().isdigit():
                    c += self.advance()
                # Optional exponent
                if self.peek() in ('e', 'E'):
                    c += self.advance()
                    if self.peek() in ('+', '-'):
                        c += self.advance()
                    while self.peek().isdigit():
                        c += self.advance()
                return Token(TokenKind.FLOAT, c, start)
            return Token(TokenKind.INT, c, start)

        # Strings
        if c == '"':
            text = ''
            while not self.at_end() and self.peek() != '"':
                ch = self.advance()
                if ch == '\\' and not self.at_end():
                    esc = self.advance()
                    if esc == 'n':
                        text += '\n'
                    elif esc == 't':
                        text += '\t'
                    elif esc == '"':
                        text += '"'
                    elif esc == '\\':
                        text += '\\'
                    else:
                        text += esc
                else:
                    text += ch
            self.advance()  # consume closing quote
            return Token(TokenKind.STRING, text, start)

        return Token(TokenKind.EOF, '', start)

    def tokenize(self):
        tokens = []
        while True:
            tok = self.next_token()
            tokens.append(tok)
            if tok.kind == TokenKind.EOF:
                break
        return tokens


# AST node types (using simple dicts instead of dataclasses)
def make_param(name, ty):
    return {'type': 'Param', 'name': name, 'ty': ty}

def make_fn_def(name, params, return_type, body, type_params=None, is_async=False, is_generator=False):
    return {'type': 'FnDef', 'name': name, 'params': params, 'return_type': return_type, 'body': body, 'type_params': type_params or [], 'is_async': is_async, 'is_generator': is_generator}

def make_gen_fn_def(name, params, yield_type, body, type_params=None):
    """Create a generator function definition (fn* syntax)."""
    return {'type': 'GenFnDef', 'name': name, 'params': params, 'yield_type': yield_type, 'body': body, 'type_params': type_params or []}

def make_enum_def(name, variants):
    return {'type': 'EnumDef', 'name': name, 'variants': variants}

def make_struct_def(name, fields, type_params=None):
    return {'type': 'StructDef', 'name': name, 'fields': fields, 'type_params': type_params or []}

def make_impl_def(type_name, methods, trait_name=None, assoc_types=None):
    return {'type': 'ImplDef', 'type_name': type_name, 'methods': methods, 'trait_name': trait_name, 'assoc_types': assoc_types or {}}

def make_struct_lit(name, field_inits):
    return {'type': 'StructLit', 'name': name, 'field_inits': field_inits}

def make_field_access(obj, field):
    return {'type': 'FieldAccess', 'object': obj, 'field': field}

def make_method_call(obj, method, args):
    return {'type': 'MethodCall', 'object': obj, 'method': method, 'args': args}

def make_let_stmt(name, ty, value):
    return {'type': 'LetStmt', 'name': name, 'ty': ty, 'value': value}

def make_return_stmt(value):
    return {'type': 'ReturnStmt', 'value': value}

def make_expr_stmt(expr):
    return {'type': 'ExprStmt', 'expr': expr}

def make_assign_stmt(name, value):
    return {'type': 'AssignStmt', 'name': name, 'value': value}

def make_if_expr(condition, then_block, else_block):
    return {'type': 'IfExpr', 'condition': condition, 'then_block': then_block, 'else_block': else_block}

def make_while_expr(condition, body):
    return {'type': 'WhileExpr', 'condition': condition, 'body': body}

def make_for_expr(var_name, start, end, body):
    return {'type': 'ForExpr', 'var_name': var_name, 'start': start, 'end': end, 'body': body}

def make_match_expr(scrutinee, arms):
    return {'type': 'MatchExpr', 'scrutinee': scrutinee, 'arms': arms}

def make_match_arm(pattern, result):
    return {'type': 'MatchArm', 'pattern': pattern, 'result': result}

def make_binary_expr(op, left, right):
    return {'type': 'BinaryExpr', 'op': op, 'left': left, 'right': right}

def make_call_expr(func, args):
    return {'type': 'CallExpr', 'func': func, 'args': args}

def make_ident_expr(name):
    return {'type': 'IdentExpr', 'name': name}

def make_int_expr(value):
    return {'type': 'IntExpr', 'value': value}

def make_float_expr(value):
    return {'type': 'FloatExpr', 'value': value}

def make_bool_expr(value):
    return {'type': 'BoolExpr', 'value': value}

def make_string_expr(value):
    return {'type': 'StringExpr', 'value': value}

def make_fstring_expr(parts, exprs):
    return {'type': 'FStringExpr', 'parts': parts, 'exprs': exprs}

def make_enum_variant_expr(enum_name, variant):
    return {'type': 'EnumVariantExpr', 'enum_name': enum_name, 'variant': variant}

def make_block(stmts, expr=None):
    return {'type': 'Block', 'stmts': stmts, 'expr': expr}

def make_closure_expr(params, body):
    return {'type': 'ClosureExpr', 'params': params, 'body': body}

def make_async_closure_expr(params, body):
    return {'type': 'AsyncClosureExpr', 'params': params, 'body': body}

def make_yield_expr(inner):
    return {'type': 'YieldExpr', 'inner': inner}

def make_actor_def(name, state_vars, init_block, handlers):
    return {'type': 'ActorDef', 'name': name, 'state_vars': state_vars, 'init_block': init_block, 'handlers': handlers}

def make_receive_handler(name, params, return_type, body):
    return {'type': 'ReceiveHandler', 'name': name, 'params': params, 'return_type': return_type, 'body': body}

def make_spawn_expr(actor_name, args):
    return {'type': 'SpawnExpr', 'actor_name': actor_name, 'args': args}

def make_send_expr(target, message_name, args):
    return {'type': 'SendExpr', 'target': target, 'message_name': message_name, 'args': args}

def make_ask_expr(target, message_name, args):
    return {'type': 'AskExpr', 'target': target, 'message_name': message_name, 'args': args}

def make_specialist_def(name, config, state_vars, handlers):
    return {'type': 'SpecialistDef', 'name': name, 'config': config, 'state_vars': state_vars, 'handlers': handlers}

def make_hive_def(name, specialists, router, strategy):
    return {'type': 'HiveDef', 'name': name, 'specialists': specialists, 'router': router, 'strategy': strategy}

def make_infer_expr(prompt, options):
    return {'type': 'InferExpr', 'prompt': prompt, 'options': options}


class Parser:
    def __init__(self, tokens):
        self.tokens = tokens
        self.pos = 0

    def current(self):
        if self.pos >= len(self.tokens):
            return Token(TokenKind.EOF, '', 0)
        return self.tokens[self.pos]

    def advance(self):
        tok = self.current()
        self.pos += 1
        return tok

    def expect(self, kind):
        tok = self.advance()
        if tok.kind != kind:
            raise SyntaxError(f"Expected {kind}, got {tok.kind} '{tok.text}' at pos {tok.pos}")
        return tok

    def check(self, kind):
        return self.current().kind == kind

    def parse_program(self):
        items = []
        while not self.check(TokenKind.EOF):
            items.append(self.parse_item())
        return items

    def parse_attributes(self):
        """Parse #[attr], #[attr(args)], #[attr = "value"] attributes."""
        attrs = []
        while self.check(TokenKind.HASH):
            self.advance()  # consume #
            self.expect(TokenKind.LBRACKET)

            # Parse attribute name
            attr_name = self.expect(TokenKind.IDENT).text
            attr_args = None
            attr_value = None

            # Check for #[name(args)]
            if self.check(TokenKind.LPAREN):
                self.advance()
                args = []
                while not self.check(TokenKind.RPAREN):
                    if self.check(TokenKind.IDENT):
                        arg = self.advance().text
                        # Check for key = value in args
                        if self.check(TokenKind.EQ):
                            self.advance()
                            if self.check(TokenKind.STRING):
                                val = self.advance().text
                                args.append(f"{arg}={val}")
                            elif self.check(TokenKind.IDENT):
                                val = self.advance().text
                                args.append(f"{arg}={val}")
                            else:
                                args.append(arg)
                        else:
                            args.append(arg)
                    elif self.check(TokenKind.STRING):
                        args.append(self.advance().text)
                    if self.check(TokenKind.COMMA):
                        self.advance()
                self.expect(TokenKind.RPAREN)
                attr_args = args
            # Check for #[name = "value"]
            elif self.check(TokenKind.EQ):
                self.advance()
                attr_value = self.expect(TokenKind.STRING).text

            self.expect(TokenKind.RBRACKET)
            attrs.append({'name': attr_name, 'args': attr_args, 'value': attr_value})

        return attrs

    def parse_item(self):
        # Parse any attributes first
        attrs = self.parse_attributes()

        # Track visibility: pub = public, otherwise private
        is_pub = False
        if self.check(TokenKind.KW_PUB):
            self.advance()
            is_pub = True

        item = None
        if self.check(TokenKind.KW_ASYNC):
            self.advance()
            item = self.parse_fn_def(is_async=True)
        elif self.check(TokenKind.KW_FN):
            # Check for generator function: fn*
            fn_tok = self.advance()
            if self.check(TokenKind.STAR):
                self.advance()  # Consume *
                item = self.parse_gen_fn_def()
            else:
                self.pos -= 1  # Back up to fn
                item = self.parse_fn_def()
        elif self.check(TokenKind.KW_ENUM):
            item = self.parse_enum_def()
        elif self.check(TokenKind.KW_STRUCT):
            item = self.parse_struct_def()
        elif self.check(TokenKind.KW_IMPL):
            item = self.parse_impl_def()
        elif self.check(TokenKind.KW_TRAIT):
            item = self.parse_trait_def()
        elif self.check(TokenKind.KW_ACTOR):
            item = self.parse_actor_def()
        elif self.check(TokenKind.KW_SPECIALIST):
            item = self.parse_specialist_def()
        elif self.check(TokenKind.KW_HIVE):
            item = self.parse_hive_def()
        elif self.check(TokenKind.KW_MOD):
            item = self.parse_mod_def()
        elif self.check(TokenKind.KW_USE):
            item = self.parse_use_def()
        else:
            raise SyntaxError(f"Unexpected token {self.current().kind}")

        # Add visibility and attributes to item
        if item and isinstance(item, dict):
            item['is_pub'] = is_pub
            if attrs:
                item['attrs'] = attrs
        return item

    def parse_fn_def(self, is_async=False):
        self.expect(TokenKind.KW_FN)
        name = self.expect(TokenKind.IDENT).text

        # Parse optional type parameters with bounds: <T, U: Trait, const N: i64, ...>
        type_params = []
        type_bounds = {}  # Map from type param to list of bounds
        if self.check(TokenKind.LT):
            self.advance()
            while not self.check(TokenKind.GT):
                if self.check(TokenKind.KW_CONST):
                    # const N: Type
                    self.advance()  # consume 'const'
                    const_name = self.expect(TokenKind.IDENT).text
                    self.expect(TokenKind.COLON)
                    self.parse_type()  # consume type (we don't use it yet)
                    type_params.append(f"const:{const_name}")
                else:
                    param_name = self.expect(TokenKind.IDENT).text
                    type_params.append(param_name)
                    # Check for bounds: T: Trait or T: Trait1 + Trait2
                    if self.check(TokenKind.COLON):
                        self.advance()
                        bounds = [self.parse_type()]
                        while self.check(TokenKind.PLUS):
                            self.advance()
                            bounds.append(self.parse_type())
                        type_bounds[param_name] = bounds
                if self.check(TokenKind.COMMA):
                    self.advance()
            self.expect(TokenKind.GT)

        self.expect(TokenKind.LPAREN)
        params = self.parse_params()
        self.expect(TokenKind.RPAREN)

        return_type = 'void'
        if self.check(TokenKind.ARROW):
            self.advance()
            return_type = self.parse_type()

        # Parse optional where clause: where T: Trait, U: OtherTrait
        where_bounds = {}
        if self.check(TokenKind.KW_WHERE):
            self.advance()
            while not self.check(TokenKind.LBRACE):
                param_name = self.expect(TokenKind.IDENT).text
                self.expect(TokenKind.COLON)
                bounds = [self.parse_type()]
                while self.check(TokenKind.PLUS):
                    self.advance()
                    bounds.append(self.parse_type())
                where_bounds[param_name] = bounds
                if self.check(TokenKind.COMMA):
                    self.advance()
                else:
                    break

        # Merge bounds
        all_bounds = {**type_bounds, **where_bounds}

        body = self.parse_block()
        fn_def = make_fn_def(name, params, return_type, body, type_params, is_async)
        if all_bounds:
            fn_def['type_bounds'] = all_bounds
        return fn_def

    def parse_gen_fn_def(self):
        """Parse generator function: fn* name<T>(...) -> Stream<T> { yield expr; }"""
        name = self.expect(TokenKind.IDENT).text

        # Parse optional type parameters
        type_params = []
        if self.check(TokenKind.LT):
            self.advance()
            while not self.check(TokenKind.GT):
                type_params.append(self.expect(TokenKind.IDENT).text)
                if self.check(TokenKind.COMMA):
                    self.advance()
            self.expect(TokenKind.GT)

        self.expect(TokenKind.LPAREN)
        params = self.parse_params()
        self.expect(TokenKind.RPAREN)

        # Parse yield type: -> Stream<T> or -> T (we extract T)
        yield_type = 'i64'
        if self.check(TokenKind.ARROW):
            self.advance()
            ret_type = self.parse_type()
            # Extract inner type from Stream<T>
            if ret_type.startswith('Stream<') and ret_type.endswith('>'):
                yield_type = ret_type[7:-1]
            else:
                yield_type = ret_type

        body = self.parse_block()
        return make_gen_fn_def(name, params, yield_type, body, type_params)

    def parse_params(self):
        params = []
        while not self.check(TokenKind.RPAREN):
            # P1.3: Handle &self and &mut self
            if self.check(TokenKind.AMP):
                self.advance()
                is_mut = False
                if self.check(TokenKind.KW_MUT):
                    self.advance()
                    is_mut = True
                if self.check(TokenKind.KW_SELF):
                    self.advance()
                    name = 'self'
                    ty = '&mut Self' if is_mut else '&Self'
                    params.append(make_param(name, ty))
                    if self.check(TokenKind.COMMA):
                        self.advance()
                    continue
                else:
                    raise SyntaxError(f"Unexpected token after &: expected 'self' or 'mut self'")
            # Accept either identifier or 'self' keyword as parameter name
            elif self.check(TokenKind.KW_SELF):
                name = 'self'
                self.advance()
                # Check for explicit type or bare self
                if self.check(TokenKind.COLON):
                    self.advance()
                    ty = self.parse_type()
                else:
                    ty = 'Self'  # Default type for bare self
            else:
                name = self.expect(TokenKind.IDENT).text
                self.expect(TokenKind.COLON)
                ty = self.parse_type()
            params.append(make_param(name, ty))
            if self.check(TokenKind.COMMA):
                self.advance()
        return params

    def parse_type(self):
        # Check for raw pointer types: *const T, *mut T
        if self.check(TokenKind.STAR):
            self.advance()
            if self.check(TokenKind.KW_CONST):
                self.advance()
                modifier = 'const'
            elif self.check(TokenKind.KW_MUT):
                self.advance()
                modifier = 'mut'
            else:
                raise SyntaxError(f"Expected 'const' or 'mut' after '*', got '{self.current().text}'")
            inner_type = self.parse_type()
            return f"*{modifier} {inner_type}"

        # Check for reference types: &T, &mut T
        if self.check(TokenKind.AMP):
            self.advance()
            if self.check(TokenKind.KW_MUT):
                self.advance()
                inner_type = self.parse_type()
                return f"&mut {inner_type}"
            inner_type = self.parse_type()
            return f"&{inner_type}"

        # Check for Self or Self::AssocType
        if self.check(TokenKind.KW_SELF):
            self.advance()
            if self.check(TokenKind.DOUBLE_COLON):
                self.advance()
                assoc_name = self.expect(TokenKind.IDENT).text
                return f"Self::{assoc_name}"
            return "Self"

        # P1.5: Check for impl Trait return type
        if self.check(TokenKind.KW_IMPL):
            self.advance()
            trait_name = self.expect(TokenKind.IDENT).text
            # Handle generic trait: impl Trait<T>
            if self.check(TokenKind.LT):
                type_args = self._parse_type_args_with_gtgt_handling()
                trait_name = f"{trait_name}<{', '.join(type_args)}>"
            # Handle multiple bounds: impl Trait1 + Trait2
            while self.check(TokenKind.PLUS):
                self.advance()
                next_trait = self.expect(TokenKind.IDENT).text
                if self.check(TokenKind.LT):
                    next_type_args = self._parse_type_args_with_gtgt_handling()
                    next_trait = f"{next_trait}<{', '.join(next_type_args)}>"
                trait_name = f"{trait_name} + {next_trait}"
            return f"impl {trait_name}"

        # Check for tuple types: (T, U, ...)
        if self.check(TokenKind.LPAREN):
            self.advance()
            if self.check(TokenKind.RPAREN):
                self.advance()
                return "()"  # Unit type
            types = [self.parse_type()]
            while self.check(TokenKind.COMMA):
                self.advance()
                if self.check(TokenKind.RPAREN):
                    break  # Allow trailing comma
                types.append(self.parse_type())
            self.expect(TokenKind.RPAREN)
            return f"({', '.join(types)})"

        # Parse base type name (could be 'dyn' for trait objects)
        base_type = self.expect(TokenKind.IDENT).text

        # Handle 'dyn Trait' syntax
        if base_type == 'dyn':
            trait_name = self.expect(TokenKind.IDENT).text
            # Handle generic trait: dyn Trait<T>
            if self.check(TokenKind.LT):
                type_args = self._parse_type_args_with_gtgt_handling()
                return f"dyn {trait_name}<{', '.join(type_args)}>"
            return f"dyn {trait_name}"

        # Check for generic type parameters: Type<T, U, ...>
        if self.check(TokenKind.LT):
            type_args = self._parse_type_args_with_gtgt_handling()
            return f"{base_type}<{', '.join(type_args)}>"

        return base_type

    def _parse_type_args_with_gtgt_handling(self):
        """Parse type arguments, handling >> as two > tokens for nested generics."""
        self.advance()  # consume <
        type_args = []

        while True:
            # Check if we have a pending GT from a split GTGT
            if hasattr(self, '_pending_gt') and self._pending_gt:
                self._pending_gt = False
                break

            # Check for end of type args
            if self.check(TokenKind.GT):
                self.advance()
                break

            # Handle >> as two > tokens (for nested generics like Vec<Vec<i64>>)
            if self.check(TokenKind.GTGT):
                # Consume >> and set flag so outer generic sees a pending >
                self.advance()
                self._pending_gt = True
                break

            type_args.append(self.parse_type())

            if self.check(TokenKind.COMMA):
                self.advance()

        return type_args

    def parse_enum_def(self):
        self.expect(TokenKind.KW_ENUM)
        name = self.expect(TokenKind.IDENT).text
        # Parse optional type parameters: <T, U, ...>
        type_params = []
        if self.check(TokenKind.LT):
            self.advance()
            while not self.check(TokenKind.GT):
                type_params.append(self.expect(TokenKind.IDENT).text)
                if self.check(TokenKind.COMMA):
                    self.advance()
            self.expect(TokenKind.GT)
        self.expect(TokenKind.LBRACE)
        variants = []
        while not self.check(TokenKind.RBRACE):
            variant_name = self.expect(TokenKind.IDENT).text
            # Check for tuple variant: Some(T)
            variant_types = []
            if self.check(TokenKind.LPAREN):
                self.advance()
                if not self.check(TokenKind.RPAREN):
                    variant_types.append(self.parse_type())
                    while self.check(TokenKind.COMMA):
                        self.advance()
                        variant_types.append(self.parse_type())
                self.expect(TokenKind.RPAREN)
            variants.append({'name': variant_name, 'types': variant_types})
            if self.check(TokenKind.COMMA):
                self.advance()
        self.expect(TokenKind.RBRACE)
        return {'type': 'EnumDef', 'name': name, 'type_params': type_params, 'variants': variants}

    def parse_struct_def(self):
        self.expect(TokenKind.KW_STRUCT)
        name = self.expect(TokenKind.IDENT).text
        # Parse optional type parameters: <T, U, ...>
        type_params = []
        if self.check(TokenKind.LT):
            self.advance()
            while not self.check(TokenKind.GT):
                type_params.append(self.expect(TokenKind.IDENT).text)
                if self.check(TokenKind.COMMA):
                    self.advance()
            self.expect(TokenKind.GT)
        self.expect(TokenKind.LBRACE)
        fields = []
        while not self.check(TokenKind.RBRACE):
            field_name = self.expect(TokenKind.IDENT).text
            self.expect(TokenKind.COLON)
            field_type = self.parse_type()
            fields.append((field_name, field_type))
            if self.check(TokenKind.COMMA):
                self.advance()
        self.expect(TokenKind.RBRACE)
        return make_struct_def(name, fields, type_params)

    def parse_impl_def(self):
        self.expect(TokenKind.KW_IMPL)
        type_name = self.expect(TokenKind.IDENT).text

        # Check for "impl Trait for Type"
        trait_name = None
        if self.check(TokenKind.KW_FOR):
            self.advance()
            trait_name = type_name  # First ident was trait name
            type_name = self.expect(TokenKind.IDENT).text

        self.expect(TokenKind.LBRACE)
        methods = []
        assoc_types = {}
        while not self.check(TokenKind.RBRACE):
            # Skip pub keyword if present
            if self.check(TokenKind.KW_PUB):
                self.advance()
            # Check for associated type: type Name = Type;
            if self.check(TokenKind.KW_TYPE):
                self.advance()
                assoc_name = self.expect(TokenKind.IDENT).text
                self.expect(TokenKind.EQ)
                assoc_ty = self.parse_type()
                self.expect(TokenKind.SEMI)
                assoc_types[assoc_name] = assoc_ty
            elif self.check(TokenKind.KW_ASYNC):
                self.advance()
                methods.append(self.parse_fn_def(is_async=True))
            elif self.check(TokenKind.KW_FN):
                methods.append(self.parse_fn_def())
        self.expect(TokenKind.RBRACE)
        return make_impl_def(type_name, methods, trait_name, assoc_types)

    def parse_trait_def(self):
        """Parse trait definition:
        trait Name {
            type AssocType;
            fn method(self) -> RetType;
        }
        """
        self.expect(TokenKind.KW_TRAIT)
        name = self.expect(TokenKind.IDENT).text
        self.expect(TokenKind.LBRACE)
        methods = []
        assoc_types = []
        while not self.check(TokenKind.RBRACE):
            # Skip pub keyword if present
            if self.check(TokenKind.KW_PUB):
                self.advance()
            # Check for associated type declaration: type Name;
            if self.check(TokenKind.KW_TYPE):
                self.advance()
                assoc_name = self.expect(TokenKind.IDENT).text
                self.expect(TokenKind.SEMI)
                assoc_types.append(assoc_name)
            elif self.check(TokenKind.KW_FN) or self.check(TokenKind.KW_ASYNC):
                methods.append(self.parse_trait_method())
        self.expect(TokenKind.RBRACE)
        return {'type': 'TraitDef', 'name': name, 'methods': methods, 'assoc_types': assoc_types}

    def parse_trait_method(self):
        """Parse trait method signature (no body) or default method (with body)"""
        # Check for async fn
        is_async = False
        if self.check(TokenKind.KW_ASYNC):
            self.advance()
            is_async = True
        self.expect(TokenKind.KW_FN)
        name = self.expect(TokenKind.IDENT).text
        self.expect(TokenKind.LPAREN)
        params = self.parse_params()
        self.expect(TokenKind.RPAREN)

        return_type = 'void'
        if self.check(TokenKind.ARROW):
            self.advance()
            return_type = self.parse_type()

        # Check for semicolon (no body) or block (default impl)
        body = None
        if self.check(TokenKind.SEMI):
            self.advance()
        elif self.check(TokenKind.LBRACE):
            body = self.parse_block()

        return {'name': name, 'params': params, 'return_type': return_type, 'body': body, 'is_async': is_async}

    def parse_actor_def(self):
        """Parse actor definition:
        actor Name {
            var field: Type = init_value
            init(params) { body }
            receive MessageName(params) -> ReturnType { body }
        }
        """
        self.expect(TokenKind.KW_ACTOR)
        name = self.expect(TokenKind.IDENT).text
        self.expect(TokenKind.LBRACE)

        state_vars = []
        init_block = None
        handlers = []

        while not self.check(TokenKind.RBRACE):
            if self.check(TokenKind.KW_VAR):
                # State variable: var name: Type = init_value
                self.advance()
                var_name = self.expect(TokenKind.IDENT).text
                self.expect(TokenKind.COLON)
                var_type = self.parse_type()
                init_value = None
                if self.check(TokenKind.EQ):
                    self.advance()
                    init_value = self.parse_expr()
                if self.check(TokenKind.SEMI):
                    self.advance()
                state_vars.append({'name': var_name, 'ty': var_type, 'init': init_value})

            elif self.check(TokenKind.KW_INIT):
                # Init block: init(params) { body }
                self.advance()
                self.expect(TokenKind.LPAREN)
                params = self.parse_params()
                self.expect(TokenKind.RPAREN)
                init_block = {'params': params, 'body': self.parse_block()}

            elif self.check(TokenKind.KW_RECEIVE):
                # Receive handler: receive MessageName(params) -> ReturnType { body }
                self.advance()
                msg_name = self.expect(TokenKind.IDENT).text
                params = []
                if self.check(TokenKind.LPAREN):
                    self.advance()
                    params = self.parse_params()
                    self.expect(TokenKind.RPAREN)
                return_type = 'void'
                if self.check(TokenKind.ARROW):
                    self.advance()
                    return_type = self.parse_type()
                body = self.parse_block()
                handlers.append(make_receive_handler(msg_name, params, return_type, body))
            else:
                raise SyntaxError(f"Unexpected token in actor: {self.current().kind}")

        self.expect(TokenKind.RBRACE)
        return make_actor_def(name, state_vars, init_block, handlers)

    def parse_specialist_def(self):
        """Parse specialist definition - like actor but with model config and infer.
        specialist Name {
            model: "model-name",
            temperature: 0.7,
            var field: Type = init_value
            receive MessageName(params) -> ReturnType { body }
        }
        """
        self.expect(TokenKind.KW_SPECIALIST)
        name = self.expect(TokenKind.IDENT).text
        self.expect(TokenKind.LBRACE)

        config = {}
        state_vars = []
        handlers = []

        while not self.check(TokenKind.RBRACE):
            if self.check(TokenKind.IDENT):
                # Config field: model: "...", temperature: 0.7
                field_name = self.advance().text
                self.expect(TokenKind.COLON)
                if self.check(TokenKind.STRING):
                    config[field_name] = self.advance().text
                elif self.check(TokenKind.INT):
                    config[field_name] = int(self.advance().text)
                else:
                    # Parse as expression
                    config[field_name] = self.parse_expr()
                if self.check(TokenKind.COMMA):
                    self.advance()

            elif self.check(TokenKind.KW_VAR):
                # State variable
                self.advance()
                var_name = self.expect(TokenKind.IDENT).text
                self.expect(TokenKind.COLON)
                var_type = self.parse_type()
                init_value = None
                if self.check(TokenKind.EQ):
                    self.advance()
                    init_value = self.parse_expr()
                if self.check(TokenKind.SEMI):
                    self.advance()
                state_vars.append({'name': var_name, 'ty': var_type, 'init': init_value})

            elif self.check(TokenKind.KW_RECEIVE):
                # Receive handler
                self.advance()
                msg_name = self.expect(TokenKind.IDENT).text
                params = []
                if self.check(TokenKind.LPAREN):
                    self.advance()
                    params = self.parse_params()
                    self.expect(TokenKind.RPAREN)
                return_type = 'void'
                if self.check(TokenKind.ARROW):
                    self.advance()
                    return_type = self.parse_type()
                body = self.parse_block()
                handlers.append(make_receive_handler(msg_name, params, return_type, body))
            else:
                raise SyntaxError(f"Unexpected token in specialist: {self.current().kind}")

        self.expect(TokenKind.RBRACE)
        return make_specialist_def(name, config, state_vars, handlers)

    def parse_hive_def(self):
        """Parse hive definition - supervisor for specialists.
        hive Name {
            specialists: [Spec1, Spec2],
            router: RuleRouter,
            strategy: OneForOne
        }
        """
        self.expect(TokenKind.KW_HIVE)
        name = self.expect(TokenKind.IDENT).text
        self.expect(TokenKind.LBRACE)

        specialists = []
        router = None
        strategy = 'OneForOne'

        while not self.check(TokenKind.RBRACE):
            field_name = self.expect(TokenKind.IDENT).text
            self.expect(TokenKind.COLON)

            if field_name == 'specialists':
                self.expect(TokenKind.LBRACKET)
                while not self.check(TokenKind.RBRACKET):
                    specialists.append(self.expect(TokenKind.IDENT).text)
                    if self.check(TokenKind.COMMA):
                        self.advance()
                self.expect(TokenKind.RBRACKET)
            elif field_name == 'router':
                router = self.expect(TokenKind.IDENT).text
            elif field_name == 'strategy':
                strategy = self.expect(TokenKind.IDENT).text

            if self.check(TokenKind.COMMA):
                self.advance()

        self.expect(TokenKind.RBRACE)
        return make_hive_def(name, specialists, router, strategy)

    def parse_mod_def(self):
        """Parse mod name; or mod name { ... } declaration"""
        self.expect(TokenKind.KW_MOD)
        name = self.expect(TokenKind.IDENT).text

        # Check for module body: mod name { ... }
        if self.check(TokenKind.LBRACE):
            self.advance()
            items = []
            while not self.check(TokenKind.RBRACE):
                # Handle visibility modifier
                is_pub = False
                if self.check(TokenKind.KW_PUB):
                    is_pub = True
                    self.advance()

                # Parse module items
                if self.check(TokenKind.KW_FN):
                    fn_def = self.parse_fn_def()
                    fn_def['is_pub'] = is_pub
                    items.append(fn_def)
                elif self.check(TokenKind.KW_STRUCT):
                    struct_def = self.parse_struct_def()
                    struct_def['is_pub'] = is_pub
                    items.append(struct_def)
                elif self.check(TokenKind.KW_ENUM):
                    enum_def = self.parse_enum_def()
                    enum_def['is_pub'] = is_pub
                    items.append(enum_def)
                elif self.check(TokenKind.KW_IMPL):
                    items.append(self.parse_impl_def())
                elif self.check(TokenKind.KW_TRAIT):
                    trait_def = self.parse_trait_def()
                    trait_def['is_pub'] = is_pub
                    items.append(trait_def)
                elif self.check(TokenKind.KW_CONST):
                    const_def = self.parse_const_def()
                    const_def['is_pub'] = is_pub
                    items.append(const_def)
                elif self.check(TokenKind.KW_MOD):
                    # Nested modules
                    mod_def = self.parse_mod_def()
                    mod_def['is_pub'] = is_pub
                    items.append(mod_def)
                elif self.check(TokenKind.KW_USE):
                    items.append(self.parse_use_def())
                else:
                    raise SyntaxError(f"Unexpected token in module body: {self.current().kind}")
            self.expect(TokenKind.RBRACE)
            return {'type': 'ModDef', 'name': name, 'items': items}
        else:
            # Simple declaration: mod name;
            self.expect(TokenKind.SEMI)
            return {'type': 'ModDef', 'name': name, 'items': None}

    def parse_use_def(self):
        """Parse use path::item; declaration"""
        self.expect(TokenKind.KW_USE)
        path = [self.expect(TokenKind.IDENT).text]
        while self.check(TokenKind.DOUBLE_COLON):
            self.advance()
            path.append(self.expect(TokenKind.IDENT).text)
        self.expect(TokenKind.SEMI)
        return {'type': 'UseDef', 'path': path}

    def parse_block(self):
        self.expect(TokenKind.LBRACE)
        stmts = []
        expr = None

        while not self.check(TokenKind.RBRACE):
            if self.check(TokenKind.KW_LET) or self.check(TokenKind.KW_VAR):
                stmts.append(self.parse_let_stmt())
            elif self.check(TokenKind.KW_RETURN):
                stmts.append(self.parse_return_stmt())
            elif self.check(TokenKind.KW_BREAK):
                self.advance()
                if self.check(TokenKind.SEMI):
                    self.advance()
                stmts.append({'type': 'BreakStmt'})
            elif self.check(TokenKind.KW_CONTINUE):
                self.advance()
                if self.check(TokenKind.SEMI):
                    self.advance()
                stmts.append({'type': 'ContinueStmt'})
            else:
                e = self.parse_expr()
                # Check for assignment: ident = value, *ptr = value, or self.field = value
                if self.check(TokenKind.EQ):
                    if e['type'] == 'IdentExpr':
                        self.advance()  # consume =
                        value = self.parse_expr()
                        if self.check(TokenKind.SEMI):
                            self.advance()
                        stmts.append(make_assign_stmt(e['name'], value))
                    elif e['type'] == 'DerefExpr':
                        self.advance()  # consume =
                        value = self.parse_expr()
                        if self.check(TokenKind.SEMI):
                            self.advance()
                        stmts.append({'type': 'DerefAssignStmt', 'target': e, 'value': value})
                    elif e['type'] == 'FieldAccess':
                        self.advance()  # consume =
                        value = self.parse_expr()
                        if self.check(TokenKind.SEMI):
                            self.advance()
                        stmts.append({'type': 'FieldAssignStmt', 'target': e, 'value': value})
                elif self.check(TokenKind.SEMI):
                    self.advance()
                    stmts.append(make_expr_stmt(e))
                elif self.check(TokenKind.RBRACE):
                    expr = e
                else:
                    stmts.append(make_expr_stmt(e))

        self.expect(TokenKind.RBRACE)
        return make_block(stmts, expr)

    def parse_let_stmt(self):
        # Accept either 'let' or 'var'
        if self.check(TokenKind.KW_VAR):
            self.advance()
        else:
            self.expect(TokenKind.KW_LET)
        name = self.expect(TokenKind.IDENT).text
        ty = None
        if self.check(TokenKind.COLON):
            self.advance()
            ty = self.parse_type()
        self.expect(TokenKind.EQ)
        value = self.parse_expr()
        if self.check(TokenKind.SEMI):
            self.advance()
        return make_let_stmt(name, ty, value)

    def parse_return_stmt(self):
        self.expect(TokenKind.KW_RETURN)
        value = None
        if not self.check(TokenKind.SEMI) and not self.check(TokenKind.RBRACE):
            value = self.parse_expr()
        if self.check(TokenKind.SEMI):
            self.advance()
        return make_return_stmt(value)

    def parse_expr(self):
        return self.parse_or()

    def parse_or(self):
        left = self.parse_and()
        while self.check(TokenKind.PIPEPIPE):
            op = self.advance().text
            right = self.parse_and()
            left = make_binary_expr(op, left, right)
        return left

    def parse_and(self):
        left = self.parse_bitor()
        while self.check(TokenKind.AMPAMP):
            op = self.advance().text
            right = self.parse_bitor()
            left = make_binary_expr(op, left, right)
        return left

    def parse_bitor(self):
        left = self.parse_bitxor()
        while self.check(TokenKind.PIPE):
            op = self.advance().text
            right = self.parse_bitxor()
            left = make_binary_expr(op, left, right)
        return left

    def parse_bitxor(self):
        left = self.parse_bitand()
        while self.check(TokenKind.CARET):
            op = self.advance().text
            right = self.parse_bitand()
            left = make_binary_expr(op, left, right)
        return left

    def parse_bitand(self):
        left = self.parse_comparison()
        while self.check(TokenKind.AMP):
            op = self.advance().text
            right = self.parse_comparison()
            left = make_binary_expr(op, left, right)
        return left

    def parse_comparison(self):
        left = self.parse_shift()

        while self.current().kind in (TokenKind.EQEQ, TokenKind.NE, TokenKind.LT,
                                      TokenKind.GT, TokenKind.LE, TokenKind.GE):
            op = self.advance().text
            right = self.parse_shift()
            left = make_binary_expr(op, left, right)

        return left

    def parse_shift(self):
        left = self.parse_additive()

        while self.current().kind in (TokenKind.LTLT, TokenKind.GTGT):
            op = self.advance().text
            right = self.parse_additive()
            left = make_binary_expr(op, left, right)

        return left

    def parse_additive(self):
        left = self.parse_multiplicative()

        while self.current().kind in (TokenKind.PLUS, TokenKind.MINUS):
            op = self.advance().text
            right = self.parse_multiplicative()
            left = make_binary_expr(op, left, right)

        return left

    def parse_multiplicative(self):
        left = self.parse_unary()

        while self.current().kind in (TokenKind.STAR, TokenKind.SLASH, TokenKind.PERCENT):
            op = self.advance().text
            right = self.parse_unary()
            left = make_binary_expr(op, left, right)

        return left

    def parse_postfix(self):
        expr = self.parse_primary()
        while True:
            if self.check(TokenKind.DOT):
                self.advance()
                # Check for .await keyword
                if self.check(TokenKind.KW_AWAIT):
                    self.advance()
                    expr = {'type': 'AwaitExpr', 'expr': expr}
                else:
                    name = self.expect(TokenKind.IDENT).text
                    # Check if method call (followed by '(')
                    if self.check(TokenKind.LPAREN):
                        self.advance()
                        args = []
                        if not self.check(TokenKind.RPAREN):
                            args.append(self.parse_expr())
                            while self.check(TokenKind.COMMA):
                                self.advance()
                                args.append(self.parse_expr())
                        self.expect(TokenKind.RPAREN)
                        expr = make_method_call(expr, name, args)
                    else:
                        expr = make_field_access(expr, name)
            elif self.check(TokenKind.QUESTION):
                # expr? - propagate error (for Result types)
                self.advance()
                expr = {'type': 'TryExpr', 'expr': expr}
            else:
                break
        return expr

    def parse_unary(self):
        if self.check(TokenKind.BANG):
            self.advance()
            operand = self.parse_unary()
            # !x -> x == 0
            return make_binary_expr('==', operand, make_int_expr(0))
        if self.check(TokenKind.MINUS):
            self.advance()
            operand = self.parse_unary()
            return make_binary_expr('-', make_int_expr(0), operand)
        # P1.1: Address-of operator (&x, &mut x)
        if self.check(TokenKind.AMP):
            self.advance()
            is_mut = False
            if self.check(TokenKind.KW_MUT):
                self.advance()
                is_mut = True
            operand = self.parse_unary()
            return {
                'type': 'AddressOfExpr',
                'operand': operand,
                'mutable': is_mut
            }
        # P1.2: Dereference operator (*p)
        if self.check(TokenKind.STAR):
            self.advance()
            operand = self.parse_unary()
            return {
                'type': 'DerefExpr',
                'operand': operand
            }
        return self.parse_postfix()

    def parse_primary(self):
        if self.check(TokenKind.INT):
            return make_int_expr(int(self.advance().text))

        if self.check(TokenKind.FLOAT):
            return make_float_expr(float(self.advance().text))

        if self.check(TokenKind.STRING):
            return make_string_expr(self.advance().text)

        if self.check(TokenKind.FSTRING):
            return self.parse_fstring()

        if self.check(TokenKind.KW_YIELD):
            self.advance()
            inner = self.parse_expr()
            return make_yield_expr(inner)

        if self.check(TokenKind.KW_TRUE):
            self.advance()
            return make_bool_expr(True)

        if self.check(TokenKind.KW_FALSE):
            self.advance()
            return make_bool_expr(False)

        # Async block: async { ... } or async closure: async (params) => body
        if self.check(TokenKind.KW_ASYNC):
            self.advance()
            # Check for async block: async { ... }
            if self.check(TokenKind.LBRACE):
                body = self.parse_block()
                return {
                    'type': 'AsyncBlockExpr',
                    'body': body
                }
            # Otherwise it's an async closure
            return self.parse_async_closure()

        if self.check(TokenKind.KW_IF):
            return self.parse_if_expr()

        if self.check(TokenKind.KW_WHILE):
            return self.parse_while_expr()

        if self.check(TokenKind.KW_FOR):
            return self.parse_for_expr()

        if self.check(TokenKind.KW_MATCH):
            return self.parse_match_expr()

        # Rust-style closure: || expr or |x| expr or |x: i64| expr
        if self.check(TokenKind.PIPEPIPE):
            # || expr - zero params closure
            self.advance()
            if self.check(TokenKind.LBRACE):
                body = self.parse_block()
            else:
                body = self.parse_expr()
            return make_closure_expr([], body)

        if self.check(TokenKind.PIPE):
            # |params| expr - closure with params
            self.advance()
            params = []
            while not self.check(TokenKind.PIPE):
                pname = self.expect(TokenKind.IDENT).text
                pty = 'i64'  # default type
                if self.check(TokenKind.COLON):
                    self.advance()
                    pty = self.parse_type()
                params.append(make_param(pname, pty))
                if self.check(TokenKind.COMMA):
                    self.advance()
            self.expect(TokenKind.PIPE)
            if self.check(TokenKind.LBRACE):
                body = self.parse_block()
            else:
                body = self.parse_expr()
            return make_closure_expr(params, body)

        if self.check(TokenKind.LBRACE):
            return self.parse_block()

        # 'self' keyword as identifier
        if self.check(TokenKind.KW_SELF):
            self.advance()
            return make_ident_expr('self')

        # spawn ActorName or spawn ActorName(args)
        if self.check(TokenKind.KW_SPAWN):
            self.advance()
            actor_name = self.expect(TokenKind.IDENT).text
            args = []
            if self.check(TokenKind.LPAREN):
                self.advance()
                while not self.check(TokenKind.RPAREN):
                    args.append(self.parse_expr())
                    if self.check(TokenKind.COMMA):
                        self.advance()
                self.expect(TokenKind.RPAREN)
            return make_spawn_expr(actor_name, args)

        # send(target, MessageName) or send(target, MessageName(args))
        if self.check(TokenKind.KW_SEND):
            self.advance()
            self.expect(TokenKind.LPAREN)
            target = self.parse_expr()
            self.expect(TokenKind.COMMA)
            msg_name = self.expect(TokenKind.IDENT).text
            args = []
            if self.check(TokenKind.LPAREN):
                self.advance()
                while not self.check(TokenKind.RPAREN):
                    args.append(self.parse_expr())
                    if self.check(TokenKind.COMMA):
                        self.advance()
                self.expect(TokenKind.RPAREN)
            self.expect(TokenKind.RPAREN)
            return make_send_expr(target, msg_name, args)

        # ask(target, MessageName) or ask(target, MessageName(args))
        if self.check(TokenKind.KW_ASK):
            self.advance()
            self.expect(TokenKind.LPAREN)
            target = self.parse_expr()
            self.expect(TokenKind.COMMA)
            msg_name = self.expect(TokenKind.IDENT).text
            args = []
            if self.check(TokenKind.LPAREN):
                self.advance()
                while not self.check(TokenKind.RPAREN):
                    args.append(self.parse_expr())
                    if self.check(TokenKind.COMMA):
                        self.advance()
                self.expect(TokenKind.RPAREN)
            self.expect(TokenKind.RPAREN)
            return make_ask_expr(target, msg_name, args)

        # await expr - for sync bootstrap, await is a no-op
        if self.check(TokenKind.KW_AWAIT):
            self.advance()
            inner = self.parse_unary()  # Parse the awaited expression
            # For sync bootstrap, await just returns the inner expression
            return {'type': 'AwaitExpr', 'expr': inner}

        # infer(prompt) or infer(prompt, temperature: 0.7, ...)
        if self.check(TokenKind.KW_INFER):
            self.advance()
            self.expect(TokenKind.LPAREN)
            prompt = self.parse_expr()
            options = {}
            while self.check(TokenKind.COMMA):
                self.advance()
                opt_name = self.expect(TokenKind.IDENT).text
                self.expect(TokenKind.COLON)
                opt_value = self.parse_expr()
                options[opt_name] = opt_value
            self.expect(TokenKind.RPAREN)
            return make_infer_expr(prompt, options)

        if self.check(TokenKind.IDENT):
            name = self.advance().text

            # Check for :: (could be enum variant or turbofish)
            if self.check(TokenKind.DOUBLE_COLON):
                self.advance()
                # Check for turbofish generic call: name::<Type, ...>(args)
                # Supports both types (i64, String) and const values (10, 256) for const generics
                if self.check(TokenKind.LT):
                    self.advance()
                    type_args = []
                    while not self.check(TokenKind.GT):
                        if self.check(TokenKind.IDENT):
                            type_args.append(self.advance().text)
                        elif self.check(TokenKind.INT):
                            type_args.append(self.advance().text)
                        else:
                            raise Exception("Expected type or const value in generic arguments")
                        if self.check(TokenKind.COMMA):
                            self.advance()
                    self.expect(TokenKind.GT)
                    # Check for another :: for associated function: Type::<Args>::method()
                    if self.check(TokenKind.DOUBLE_COLON):
                        self.advance()
                        method_name = self.expect(TokenKind.IDENT).text
                        mangled = f"{name}_{method_name}"
                        self.expect(TokenKind.LPAREN)
                        args = []
                        while not self.check(TokenKind.RPAREN):
                            args.append(self.parse_expr())
                            if self.check(TokenKind.COMMA):
                                self.advance()
                        self.expect(TokenKind.RPAREN)
                        return {'type': 'CallExpr', 'func': mangled, 'args': args, 'type_args': type_args}
                    # Direct turbofish call: func::<Args>(...)
                    self.expect(TokenKind.LPAREN)
                    args = []
                    while not self.check(TokenKind.RPAREN):
                        args.append(self.parse_expr())
                        if self.check(TokenKind.COMMA):
                            self.advance()
                    self.expect(TokenKind.RPAREN)
                    return {'type': 'CallExpr', 'func': name, 'args': args, 'type_args': type_args}
                else:
                    # Could be enum variant (Name::Variant) or associated function (Type::function())
                    variant = self.expect(TokenKind.IDENT).text
                    # Check if it's followed by ( for function call
                    if self.check(TokenKind.LPAREN):
                        # Associated function call: Type::function(args)
                        # Mangle to Type_function
                        mangled_name = f"{name}_{variant}"
                        self.advance()  # consume (
                        args = []
                        while not self.check(TokenKind.RPAREN):
                            args.append(self.parse_expr())
                            if self.check(TokenKind.COMMA):
                                self.advance()
                        self.expect(TokenKind.RPAREN)
                        return make_call_expr(mangled_name, args)
                    return make_enum_variant_expr(name, variant)

            # Check for function call
            if self.check(TokenKind.LPAREN):
                self.advance()
                args = []
                while not self.check(TokenKind.RPAREN):
                    args.append(self.parse_expr())
                    if self.check(TokenKind.COMMA):
                        self.advance()
                self.expect(TokenKind.RPAREN)
                return make_call_expr(name, args)

            # Check for struct literal: Name { field: value, ... }
            # Must peek ahead to distinguish from block: foo { ... }
            # Struct literal has pattern: { ident : expr }
            if self.check(TokenKind.LBRACE):
                # Look ahead: if next is IDENT followed by COLON, it's a struct literal
                save_pos = self.pos
                self.advance()  # consume {
                if self.check(TokenKind.IDENT):
                    self.advance()  # consume ident
                    if self.check(TokenKind.COLON):
                        # It's a struct literal - reset and parse properly
                        self.pos = save_pos
                        self.advance()  # consume {
                        field_inits = []
                        while not self.check(TokenKind.RBRACE):
                            field_name = self.expect(TokenKind.IDENT).text
                            self.expect(TokenKind.COLON)
                            field_value = self.parse_expr()
                            field_inits.append((field_name, field_value))
                            if self.check(TokenKind.COMMA):
                                self.advance()
                        self.expect(TokenKind.RBRACE)
                        return make_struct_lit(name, field_inits)
                # Not a struct literal - reset and return just the identifier
                self.pos = save_pos

            return make_ident_expr(name)

        if self.check(TokenKind.LPAREN):
            # Could be: (expr), (params) => body, or empty ()
            save_pos = self.pos
            self.advance()  # consume (

            # Check for closure: (params) => body
            # Look for pattern: IDENT COLON or RPAREN =>
            if self.check(TokenKind.RPAREN):
                self.advance()
                if self.check(TokenKind.FAT_ARROW):
                    # () => body - no params closure
                    self.advance()
                    body = self.parse_expr()
                    return make_closure_expr([], body)
                else:
                    # Just () - restore and parse as unit/empty
                    self.pos = save_pos
                    self.advance()
                    self.expect(TokenKind.RPAREN)
                    return make_int_expr(0)  # Unit value

            # Try to parse as closure params
            might_be_closure = False
            if self.check(TokenKind.IDENT):
                peek_pos = self.pos
                self.advance()  # consume ident
                if self.check(TokenKind.COLON) or self.check(TokenKind.COMMA) or self.check(TokenKind.RPAREN):
                    might_be_closure = True
                self.pos = peek_pos  # restore

            if might_be_closure:
                # Parse as closure params
                params = []
                while not self.check(TokenKind.RPAREN):
                    pname = self.expect(TokenKind.IDENT).text
                    pty = 'i64'  # default type
                    if self.check(TokenKind.COLON):
                        self.advance()
                        pty = self.parse_type()
                    params.append(make_param(pname, pty))
                    if self.check(TokenKind.COMMA):
                        self.advance()
                self.expect(TokenKind.RPAREN)

                if self.check(TokenKind.FAT_ARROW):
                    self.advance()
                    body = self.parse_expr()
                    return make_closure_expr(params, body)
                else:
                    # Not a closure - was a parenthesized expression list?
                    # This is ambiguous, restore and try as expr
                    self.pos = save_pos

            # Regular parenthesized expression or tuple
            self.pos = save_pos
            self.advance()  # consume (
            if self.check(TokenKind.RPAREN):
                # Unit tuple ()
                self.advance()
                return {'type': 'TupleExpr', 'elements': []}
            expr = self.parse_expr()
            if self.check(TokenKind.COMMA):
                # It's a tuple
                elements = [expr]
                while self.check(TokenKind.COMMA):
                    self.advance()
                    if self.check(TokenKind.RPAREN):
                        break  # Allow trailing comma
                    elements.append(self.parse_expr())
                self.expect(TokenKind.RPAREN)
                return {'type': 'TupleExpr', 'elements': elements}
            self.expect(TokenKind.RPAREN)
            return expr

        raise SyntaxError(f"Unexpected token {self.current().kind} '{self.current().text}'")

    def parse_async_closure(self):
        """Parse async closure: async (params) => body or async () => body"""
        self.expect(TokenKind.LPAREN)
        params = []
        while not self.check(TokenKind.RPAREN):
            pname = self.expect(TokenKind.IDENT).text
            pty = 'i64'  # default type
            if self.check(TokenKind.COLON):
                self.advance()
                pty = self.parse_type()
            params.append(make_param(pname, pty))
            if self.check(TokenKind.COMMA):
                self.advance()
        self.expect(TokenKind.RPAREN)
        self.expect(TokenKind.FAT_ARROW)
        body = self.parse_expr()
        return make_async_closure_expr(params, body)

    def parse_if_expr(self):
        self.expect(TokenKind.KW_IF)

        # Check for if-let pattern: if let Pattern = expr { ... }
        if self.check(TokenKind.KW_LET):
            return self.parse_if_let_expr()

        condition = self.parse_expr()
        then_block = self.parse_block()
        else_block = None
        if self.check(TokenKind.KW_ELSE):
            self.advance()
            if self.check(TokenKind.KW_IF):
                # else if -> treat as else { if ... }
                else_block = make_block([], self.parse_if_expr())
            else:
                else_block = self.parse_block()
        return make_if_expr(condition, then_block, else_block)

    def parse_if_let_expr(self):
        """Parse if-let: if let Pattern = expr { then } else { else }
        Desugars to a match expression.
        """
        self.expect(TokenKind.KW_LET)

        # Parse pattern - could be:
        # - Some(x)
        # - Ok(x)
        # - EnumName::Variant(x)
        # - (a, b) - tuple pattern
        # - simple identifier

        pattern = self.parse_pattern()

        self.expect(TokenKind.EQ)
        scrutinee = self.parse_expr()
        then_block = self.parse_block()

        else_block = None
        if self.check(TokenKind.KW_ELSE):
            self.advance()
            if self.check(TokenKind.KW_IF):
                else_block = make_block([], self.parse_if_expr())
            else:
                else_block = self.parse_block()

        # Create IfLetExpr node
        return {
            'type': 'IfLetExpr',
            'pattern': pattern,
            'scrutinee': scrutinee,
            'then_block': then_block,
            'else_block': else_block
        }

    def parse_pattern(self):
        """Parse a pattern for if-let and match expressions."""
        # Check for enum variant pattern: Name(bindings...) or Name::Variant(bindings...)
        if self.check(TokenKind.IDENT):
            name = self.advance().text

            # Check for :: path separator
            if self.check(TokenKind.DOUBLE_COLON):
                self.advance()
                variant = self.expect(TokenKind.IDENT).text
                name = f"{name}::{variant}"

            # Check for (bindings...) - enum variant with payload
            if self.check(TokenKind.LPAREN):
                self.advance()
                bindings = []
                while not self.check(TokenKind.RPAREN):
                    # Recursively parse nested patterns
                    bindings.append(self.parse_pattern())
                    if self.check(TokenKind.COMMA):
                        self.advance()
                self.expect(TokenKind.RPAREN)
                return {
                    'type': 'EnumPattern',
                    'enum_variant': name,
                    'bindings': bindings
                }
            # Check for { fields... } - struct pattern
            elif self.check(TokenKind.LBRACE):
                self.advance()
                fields = []
                while not self.check(TokenKind.RBRACE):
                    field_name = self.expect(TokenKind.IDENT).text
                    fields.append({'name': field_name})
                    if self.check(TokenKind.COMMA):
                        self.advance()
                self.expect(TokenKind.RBRACE)
                return {
                    'type': 'StructPattern',
                    'struct_name': name,
                    'fields': fields
                }
            else:
                # Just a name - could be binding or enum variant without data
                # If name contains ::, it's an enum variant (not a binding)
                if '::' in name:
                    return {
                        'type': 'EnumPattern',
                        'enum_variant': name,
                        'bindings': []
                    }
                return {
                    'type': 'BindingPattern',
                    'name': name
                }

        elif self.check(TokenKind.UNDERSCORE):
            self.advance()
            return {'type': 'WildcardPattern'}

        elif self.check(TokenKind.LPAREN):
            # Tuple pattern: (a, b, c)
            self.advance()
            elements = []
            while not self.check(TokenKind.RPAREN):
                elements.append(self.parse_pattern())
                if self.check(TokenKind.COMMA):
                    self.advance()
            self.expect(TokenKind.RPAREN)
            return {
                'type': 'TuplePattern',
                'elements': elements
            }

        else:
            raise SyntaxError(f"Expected pattern, got {self.current().kind}")

    def parse_while_expr(self):
        self.expect(TokenKind.KW_WHILE)

        # Check for while-let: while let Pattern = expr { ... }
        if self.check(TokenKind.KW_LET):
            self.advance()
            pattern = self.parse_pattern()
            self.expect(TokenKind.EQ)
            scrutinee = self.parse_expr()
            body = self.parse_block()
            return {
                'type': 'WhileLetExpr',
                'pattern': pattern,
                'scrutinee': scrutinee,
                'body': body
            }

        condition = self.parse_expr()
        body = self.parse_block()
        return make_while_expr(condition, body)

    def parse_for_expr(self):
        self.expect(TokenKind.KW_FOR)
        var_name = self.expect(TokenKind.IDENT).text
        self.expect(TokenKind.KW_IN)

        # Check for iterator-based for loop: for x in collection { ... }
        # vs range-based: for x in 0..n { ... }
        iter_expr = self.parse_additive()

        # If we see DOTDOT, it's a range-based loop
        if self.check(TokenKind.DOTDOT):
            self.advance()
            end = self.parse_additive()
            body = self.parse_block()
            return make_for_expr(var_name, iter_expr, end, body)
        else:
            # Iterator-based for loop
            body = self.parse_block()
            return {
                'type': 'ForInExpr',
                'var': var_name,
                'iterator': iter_expr,
                'body': body
            }

    def parse_match_expr(self):
        """Parse match expression with guard support: match x { pat if cond => body }"""
        self.expect(TokenKind.KW_MATCH)
        scrutinee = self.parse_expr()
        self.expect(TokenKind.LBRACE)
        arms = []
        while not self.check(TokenKind.RBRACE):
            # Parse pattern - use parse_pattern for proper pattern parsing
            pattern = self.parse_pattern()

            # Check for optional guard: `if condition`
            guard = None
            if self.check(TokenKind.KW_IF):
                self.advance()
                guard = self.parse_expr()

            self.expect(TokenKind.FAT_ARROW)
            # Parse arm result (could be block or expression)
            if self.check(TokenKind.LBRACE):
                result = self.parse_block()
            else:
                result = self.parse_expr()

            # Create arm with optional guard
            arm = make_match_arm(pattern, result)
            if guard:
                arm['guard'] = guard
            arms.append(arm)

            # Optional comma between arms
            if self.check(TokenKind.COMMA):
                self.advance()
        self.expect(TokenKind.RBRACE)
        return make_match_expr(scrutinee, arms)

    def parse_fstring(self):
        """Parse f-string literal into parts, expressions, and format specs.
        Supports: f"Hello {name}!" and f"Value: {x:.2f}"
        """
        content = self.advance().text
        parts = []
        exprs = []
        format_specs = []  # Parallel array for format specifiers
        current = ''
        i = 0
        while i < len(content):
            c = content[i]
            if c == '{':
                parts.append(current)
                current = ''
                i += 1
                # Extract expression text until '}' or ':'
                expr_text = ''
                format_spec = ''
                depth = 1
                in_format = False
                while i < len(content):
                    ec = content[i]
                    if ec == '{':
                        depth += 1
                        if in_format:
                            format_spec += ec
                        else:
                            expr_text += ec
                    elif ec == '}':
                        depth -= 1
                        if depth == 0:
                            i += 1
                            break
                        if in_format:
                            format_spec += ec
                        else:
                            expr_text += ec
                    elif ec == ':' and depth == 1 and not in_format:
                        # Start of format specifier
                        in_format = True
                    else:
                        if in_format:
                            format_spec += ec
                        else:
                            expr_text += ec
                    i += 1

                # Parse the expression from expr_text
                expr_text = expr_text.strip()
                format_specs.append(format_spec if format_spec else None)

                # Try to parse as full expression
                if expr_text:
                    try:
                        # Lex and parse the expression
                        expr_lexer = Lexer(expr_text)
                        expr_tokens = expr_lexer.tokenize()
                        expr_parser = Parser(expr_tokens)
                        parsed_expr = expr_parser.parse_expr()
                        exprs.append(parsed_expr)
                    except:
                        # Fallback: treat as simple identifier
                        exprs.append(make_ident_expr(expr_text))
                else:
                    exprs.append(make_int_expr(0))
            else:
                current += c
                i += 1
        parts.append(current)

        # Store format specs in the fstring expr for codegen to use
        result = make_fstring_expr(parts, exprs)
        result['format_specs'] = format_specs
        return result


class CodeGen:
    def __init__(self):
        self.output = []
        self.temp_counter = 0
        self.label_counter = 0
        self.string_counter = 0
        self.local_counter = 0
        self.closure_counter = 0
        self.locals = {}  # name -> llvm_value
        self.var_types = {}  # name -> type_name (for method call resolution)
        self.enums = {
            # Built-in enums: Option<T> and Result<T,E>
            # Option: None=0, Some=1
            'Option': {'None': 0, 'Some': 1},
            # Result: Err=0, Ok=1 (matches runtime convention)
            'Result': {'Err': 0, 'Ok': 1},
        }   # enum_name -> {variant: index}
        self.structs = {}  # struct_name -> [(field_name, field_type), ...]
        self.actors = {}  # actor_name -> ActorDef (for spawn/send/ask resolution)
        self.specialists = {}  # specialist_name -> SpecialistDef
        self.hives = {}   # hive_name -> HiveDef
        self.traits = {}  # trait_name -> TraitDef
        self.trait_impls = {}  # (trait_name, type_name) -> ImplDef
        self.current_fn_return_type = 'i64'
        self.string_constants = []  # List of (label, value) pairs
        self.module_name = 'module'  # Set externally for unique string names
        self.loop_stack = []  # Stack of (continue_label, break_label) for break/continue
        self.pending_closures = []  # List of closure definitions to emit after main code
        # Generic function support
        self.generic_fns = {}  # name -> FnDef (for generic functions)
        self.pending_instantiations = []  # List of (mangled_name, fn_def, type_args)
        self.instantiated = set()  # Set of mangled names already generated
        # Associated types support
        self.assoc_types = {}  # (impl_type, assoc_name) -> concrete_type
        self.current_impl_type = None  # Type being implemented (for Self resolution)
        # Const generics support
        self.const_params = {}  # const_param_name -> literal_value (for current instantiation)
        # Track function names for function pointer references
        self.functions = set()
        # Visibility enforcement (34.3.1)
        self.public_items = {}  # module_name -> set of public item names
        self.item_visibility = {}  # (module_name, item_name) -> bool (is_pub)
        self.current_module = None  # Current module being compiled

    def register_item_visibility(self, module_name, item_name, is_pub):
        """Register an item's visibility for cross-module access checking."""
        if module_name not in self.public_items:
            self.public_items[module_name] = set()
        if is_pub:
            self.public_items[module_name].add(item_name)
        self.item_visibility[(module_name, item_name)] = is_pub

    def check_visibility(self, from_module, target_module, item_name):
        """Check if item from target_module is accessible from from_module.
        Private-by-default: items without 'pub' are only accessible from same module."""
        # Same module access is always allowed
        if from_module == target_module:
            return True
        # Check if item is public in target module
        if target_module in self.public_items:
            if item_name in self.public_items[target_module]:
                return True
        # Check explicit visibility
        key = (target_module, item_name)
        if key in self.item_visibility:
            return self.item_visibility[key]
        # Default: private (not accessible)
        return False

    def visibility_error(self, from_module, target_module, item_name):
        """Generate visibility error message."""
        return f"Error: '{item_name}' in module '{target_module}' is private and cannot be accessed from '{from_module}'"

    def new_temp(self):
        t = f"%t{self.temp_counter}"
        self.temp_counter += 1
        return t

    def new_label(self, prefix):
        l = f"{prefix}{self.label_counter}"
        self.label_counter += 1
        return l

    def extract_pattern_bindings(self, pattern, ptr_val, offset_base=0):
        """Recursively extract bindings from a pattern.

        Args:
            pattern: The pattern AST node
            ptr_val: LLVM value of the pointer to the data structure
            offset_base: Starting offset within the structure
        """
        pat_type = pattern.get('type') if pattern else None

        if pat_type == 'BindingPattern':
            # Simple binding - extract value at current offset
            bname = pattern['name']
            local_suffix = self.local_counter
            self.local_counter += 1
            slot = f'%local.{bname}.{local_suffix}'
            self.locals[bname] = slot
            self.emit(f'  {slot} = alloca i64')
            if offset_base == 0:
                # Directly load from ptr
                val = self.new_temp()
                self.emit(f'  {val} = load i64, ptr {ptr_val}')
            else:
                gep = self.new_temp()
                self.emit(f'  {gep} = getelementptr i8, ptr {ptr_val}, i64 {offset_base}')
                val = self.new_temp()
                self.emit(f'  {val} = load i64, ptr {gep}')
            self.emit(f'  store i64 {val}, ptr {slot}')

        elif pat_type == 'EnumPattern':
            # Nested enum pattern - extract payload ptr and recurse
            bindings = pattern.get('bindings', [])
            # Payload is at offset 8 (after discriminant)
            payload_offset = offset_base + 8
            for j, binding in enumerate(bindings):
                # Each binding field is at payload_offset + j*8
                binding_offset = payload_offset + j * 8
                if binding.get('type') == 'BindingPattern':
                    bname = binding['name']
                    local_suffix = self.local_counter
                    self.local_counter += 1
                    slot = f'%local.{bname}.{local_suffix}'
                    self.locals[bname] = slot
                    self.emit(f'  {slot} = alloca i64')
                    gep = self.new_temp()
                    self.emit(f'  {gep} = getelementptr i8, ptr {ptr_val}, i64 {binding_offset}')
                    val = self.new_temp()
                    self.emit(f'  {val} = load i64, ptr {gep}')
                    self.emit(f'  store i64 {val}, ptr {slot}')
                elif binding.get('type') == 'EnumPattern':
                    # Nested enum - extract inner ptr and recurse
                    gep = self.new_temp()
                    self.emit(f'  {gep} = getelementptr i8, ptr {ptr_val}, i64 {binding_offset}')
                    inner_val = self.new_temp()
                    self.emit(f'  {inner_val} = load i64, ptr {gep}')
                    inner_ptr = self.new_temp()
                    self.emit(f'  {inner_ptr} = inttoptr i64 {inner_val} to ptr')
                    # Recurse with the inner enum
                    self.extract_pattern_bindings(binding, inner_ptr, 0)

        elif pat_type == 'TuplePattern':
            # Tuple pattern - extract each element
            elements = pattern.get('elements', [])
            for j, elem in enumerate(elements):
                elem_offset = offset_base + j * 8
                self.extract_pattern_bindings(elem, ptr_val, elem_offset)

    def emit(self, line):
        self.output.append(line)

    def type_to_llvm(self, ty):
        # Apply type substitution first for monomorphization
        if hasattr(self, 'type_subst') and self.type_subst and ty in self.type_subst:
            ty = self.type_subst[ty]

        if ty in ('i64', 'bool'):
            return 'i64'
        if ty == 'f64':
            return 'double'
        if ty == 'void':
            return 'void'
        # Treat type parameters as i64 (monomorphization placeholder)
        if hasattr(self, 'current_type_params') and ty in self.current_type_params:
            return 'i64'
        # Reference types (&T, &mut T, &Self, &mut Self) are pointers, stored as i64
        if ty.startswith('&'):
            return 'i64'
        # Self resolves to i64 (struct pointer)
        if ty == 'Self' or ty.startswith('Self::'):
            return 'i64'
        # impl Trait is i64 (fat pointer or erased type)
        if ty.startswith('impl '):
            return 'i64'
        # Tuple types are pointers stored as i64
        if ty.startswith('(') and ty.endswith(')'):
            return 'i64'
        # Enum types (Option, Result, etc.) are represented as i64 (pointer-as-int)
        if ty.startswith('Option<') or ty.startswith('Result<'):
            return 'i64'
        # Generic types like Vec<T>, HashMap<K,V> are pointers stored as i64
        if '<' in ty:
            return 'i64'
        # Struct/enum names without generics - treat as pointer stored as i64
        if ty[0].isupper() and ty.isidentifier():
            return 'i64'
        return 'ptr'

    def resolve_type(self, ty):
        """Resolve Self and Self::AssocType to concrete types"""
        if not ty:
            return ty
        # Check for Self::AssocName pattern
        if ty.startswith("Self::"):
            assoc_name = ty[6:]  # Skip "Self::"
            if self.current_impl_type:
                key = (self.current_impl_type, assoc_name)
                if key in self.assoc_types:
                    return self.assoc_types[key]
            return ty  # Not resolved, return as-is
        # Check for plain Self
        if ty == "Self":
            if self.current_impl_type:
                return self.current_impl_type
        return ty

    def add_string_constant(self, value):
        label = f"@.str.{self.module_name}.{self.string_counter}"
        self.string_counter += 1
        self.string_constants.append((label, value))
        return label

    def load_module(self, mod_name):
        """Load and parse a module file, adding its items to the current context."""
        import os
        # Track loaded modules to avoid circular imports
        if not hasattr(self, 'loaded_modules'):
            self.loaded_modules = set()
        if mod_name in self.loaded_modules:
            return  # Already loaded
        self.loaded_modules.add(mod_name)

        # Find module file (mod_name.sx in current directory or src/)
        candidates = [
            f"{mod_name}.sx",
            f"src/{mod_name}.sx",
            f"{mod_name}/mod.sx",
            f"src/{mod_name}/mod.sx"
        ]

        mod_path = None
        for path in candidates:
            if os.path.exists(path):
                mod_path = path
                break

        if not mod_path:
            print(f"Warning: Module '{mod_name}' not found (tried: {candidates})")
            return

        # Parse the module
        with open(mod_path, 'r') as f:
            source = f.read()

        lexer = Lexer(source)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        mod_items = parser.parse_program()

        # Store module items for later code generation
        if not hasattr(self, 'module_items'):
            self.module_items = {}
        self.module_items[mod_name] = mod_items

        # Register module's types, functions, etc.
        for item in mod_items:
            if item['type'] == 'EnumDef':
                # Handle both old format (list of strings) and new format (list of dicts)
                variants = item['variants']
                if variants and isinstance(variants[0], dict):
                    self.enums[f"{mod_name}::{item['name']}"] = {v['name']: i for i, v in enumerate(variants)}
                else:
                    self.enums[f"{mod_name}::{item['name']}"] = {v: i for i, v in enumerate(variants)}
                self.enums[item['name']] = self.enums[f"{mod_name}::{item['name']}"]
            elif item['type'] == 'StructDef':
                self.structs[f"{mod_name}::{item['name']}"] = item['fields']
                self.structs[item['name']] = item['fields']
            elif item['type'] == 'FnDef':
                # Store function for generation when needed
                self.functions.add(f"{mod_name}_{item['name']}")
                self.functions.add(item['name'])
            elif item['type'] == 'TraitDef':
                self.traits[item['name']] = item

    def import_from_path(self, path):
        """Import items from a module path like ['std', 'io']."""
        if len(path) < 1:
            return

        mod_name = path[0]
        self.load_module(mod_name)

        # If path has more components, it specifies what to import
        # e.g., use std::io -> imports all from std/io
        # e.g., use std::io::println -> imports just println
        if len(path) > 1:
            # Mark specific items as imported into current namespace
            if not hasattr(self, 'imported_items'):
                self.imported_items = {}
            full_path = '::'.join(path)
            self.imported_items[path[-1]] = full_path

    def generate(self, items):
        # Header
        self.emit('; ModuleID = "simplex_program"')
        self.emit(f'target triple = "{get_target_triple()}"')
        self.emit('')

        # External declarations
        self.emit('declare ptr @malloc(i64)')
        self.emit('declare void @free(ptr)')
        self.emit('declare void @intrinsic_println(ptr)')
        self.emit('declare void @intrinsic_print(ptr)')
        self.emit('declare void @print_i64(i64)')            # Print integer
        self.emit('declare void @print_string(i64)')         # Print string
        self.emit('declare ptr @intrinsic_int_to_string(i64)')
        self.emit('; Format string intrinsics for f-strings')
        self.emit('declare i64 @format_f64(i64, i64)')       # format_f64(value, precision) -> string
        self.emit('declare i64 @format_hex(i64)')            # format_hex(value) -> string
        self.emit('declare i64 @format_hex_upper(i64)')      # format_hex_upper(value) -> string
        self.emit('declare i64 @format_binary(i64)')         # format_binary(value) -> string
        self.emit('declare i64 @format_padded(i64, i64, i64)') # format_padded(value, width, pad_char)
        self.emit('; Display and Debug traits (34.3.3)')
        self.emit('declare i64 @display_format(i64)')        # Display::fmt -> String
        self.emit('declare i64 @debug_format(i64)')          # Debug::fmt -> String
        self.emit('declare i64 @intrinsic_int_to_hex(i64)')  # int to hex string
        self.emit('declare i64 @intrinsic_int_to_binary(i64)') # int to binary string
        self.emit('declare void @panic(ptr)')                # panic with message
        self.emit('@str_assertion_failed = private constant [17 x i8] c"assertion failed\\00"')
        self.emit('@str_assertion_eq_failed = private constant [20 x i8] c"assertion eq failed\\00"')
        self.emit('declare ptr @intrinsic_string_new(ptr)')
        self.emit('declare ptr @intrinsic_string_from_char(i64)')
        self.emit('declare i64 @intrinsic_string_len(ptr)')
        self.emit('declare ptr @intrinsic_string_concat(ptr, ptr)')
        self.emit('declare ptr @intrinsic_string_slice(ptr, i64, i64)')
        self.emit('declare i64 @intrinsic_string_char_at(ptr, i64)')
        self.emit('declare i1 @intrinsic_string_eq(ptr, ptr)')
        self.emit('declare ptr @intrinsic_vec_new()')
        self.emit('declare void @intrinsic_vec_push(ptr, ptr)')
        self.emit('declare ptr @intrinsic_vec_get(ptr, i64)')
        self.emit('declare i64 @intrinsic_vec_len(ptr)')
        self.emit('declare void @intrinsic_vec_set(ptr, i64, ptr)')
        self.emit('declare ptr @intrinsic_vec_pop(ptr)')
        self.emit('declare void @intrinsic_vec_clear(ptr)')
        self.emit('declare i64 @intrinsic_vec_capacity(ptr)')
        self.emit('declare void @intrinsic_vec_reserve(ptr, i64)')
        self.emit('; Option<T> type: tag(0)=None, tag(1)=Some with value at offset 8')
        self.emit('declare i64 @option_none()')
        self.emit('declare i64 @option_some(i64)')
        self.emit('declare i64 @option_is_some(i64)')
        self.emit('declare i64 @option_is_none(i64)')
        self.emit('declare i64 @option_unwrap(i64)')
        self.emit('declare i64 @option_unwrap_or(i64, i64)')
        self.emit('declare i64 @option_map(i64, i64)')      # option_map(opt, fn_ptr)
        self.emit('; Result<T,E> type: tag(0)=Err, tag(1)=Ok with value at offset 8')
        self.emit('declare i64 @result_ok(i64)')
        self.emit('declare i64 @result_err(i64)')
        self.emit('declare i64 @result_is_ok(i64)')
        self.emit('declare i64 @result_is_err(i64)')
        self.emit('declare i64 @result_unwrap(i64)')
        self.emit('declare i64 @result_unwrap_err(i64)')
        self.emit('declare i64 @result_unwrap_or(i64, i64)')
        self.emit('declare i64 @result_map(i64, i64)')      # result_map(res, fn_ptr)
        self.emit('declare i64 @result_map_err(i64, i64)')  # result_map_err(res, fn_ptr)
        self.emit('; HashMap<K,V> type')
        self.emit('declare i64 @hashmap_new()')
        self.emit('declare i64 @hashmap_with_capacity(i64)')
        self.emit('declare void @hashmap_insert(i64, i64, i64)')  # hashmap_insert(map, key, value)
        self.emit('declare i64 @hashmap_get(i64, i64)')           # hashmap_get(map, key) -> Option<value>
        self.emit('declare i64 @hashmap_remove(i64, i64)')        # hashmap_remove(map, key) -> Option<value>
        self.emit('declare i64 @hashmap_contains(i64, i64)')      # hashmap_contains(map, key) -> bool
        self.emit('declare i64 @hashmap_len(i64)')
        self.emit('declare void @hashmap_clear(i64)')
        self.emit('declare i64 @hashmap_keys(i64)')               # Returns Vec<K>
        self.emit('declare i64 @hashmap_values(i64)')             # Returns Vec<V>
        self.emit('declare void @hashmap_free(i64)')
        self.emit('declare ptr @store_ptr(ptr, i64, ptr)')
        self.emit('declare ptr @store_i64(ptr, i64, i64)')
        self.emit('declare ptr @load_ptr(ptr, i64)')
        self.emit('declare i64 @load_i64(ptr, i64)')
        self.emit('declare i64 @intrinsic_string_to_int(ptr)')
        self.emit('declare ptr @intrinsic_get_args()')
        self.emit('declare ptr @intrinsic_read_file(ptr)')
        self.emit('declare void @intrinsic_write_file(ptr, ptr)')
        self.emit('; AI intrinsics (mock implementation)')
        self.emit('declare ptr @intrinsic_ai_infer(ptr, ptr, i64)')
        self.emit('declare ptr @intrinsic_ai_embed(ptr)')
        self.emit('; Timing intrinsics for performance measurement')
        self.emit('declare i64 @intrinsic_get_time_ms()')
        self.emit('declare i64 @intrinsic_get_time_us()')
        self.emit('; Arena allocator intrinsics')
        self.emit('declare ptr @intrinsic_arena_create(i64)')
        self.emit('declare ptr @intrinsic_arena_alloc(ptr, i64)')
        self.emit('declare void @intrinsic_arena_reset(ptr)')
        self.emit('declare void @intrinsic_arena_free(ptr)')
        self.emit('declare i64 @intrinsic_arena_used(ptr)')
        self.emit('; StringBuilder intrinsics for efficient string concatenation')
        self.emit('declare ptr @intrinsic_sb_new()')
        self.emit('declare ptr @intrinsic_sb_new_cap(i64)')
        self.emit('declare void @intrinsic_sb_append(ptr, ptr)')
        self.emit('declare void @intrinsic_sb_append_char(ptr, i64)')
        self.emit('declare void @intrinsic_sb_append_i64(ptr, i64)')
        self.emit('declare ptr @intrinsic_sb_to_string(ptr)')
        self.emit('declare void @intrinsic_sb_clear(ptr)')
        self.emit('declare void @intrinsic_sb_free(ptr)')
        self.emit('declare i64 @intrinsic_sb_len(ptr)')
        self.emit('; Error handling')
        self.emit('declare void @intrinsic_panic(ptr)')
        self.emit('; Test framework (Phase 34.11.12)')
        self.emit('declare i64 @test_runner_new()')
        self.emit('declare void @test_runner_add(i64, i64, i64)')      # add(runner, name, fn_ptr)
        self.emit('declare i64 @test_runner_run(i64)')                 # run all tests
        self.emit('declare i64 @test_runner_run_parallel(i64, i64)')   # run_parallel(runner, threads)
        self.emit('declare i64 @test_runner_passed(i64)')              # count passed
        self.emit('declare i64 @test_runner_failed(i64)')              # count failed
        self.emit('declare i64 @test_runner_skipped(i64)')             # count skipped
        self.emit('declare void @test_runner_free(i64)')
        self.emit('; Assertion macros')
        self.emit('declare void @assert_true(i64, i64)')               # assert_true(cond, msg)
        self.emit('declare void @assert_false(i64, i64)')              # assert_false(cond, msg)
        self.emit('declare void @assert_eq(i64, i64, i64)')            # assert_eq(a, b, msg)
        self.emit('declare void @assert_ne(i64, i64, i64)')            # assert_ne(a, b, msg)
        self.emit('declare void @assert_lt(i64, i64, i64)')            # assert_lt(a, b, msg)
        self.emit('declare void @assert_le(i64, i64, i64)')            # assert_le(a, b, msg)
        self.emit('declare void @assert_gt(i64, i64, i64)')            # assert_gt(a, b, msg)
        self.emit('declare void @assert_ge(i64, i64, i64)')            # assert_ge(a, b, msg)
        self.emit('; Test discovery')
        self.emit('declare i64 @test_discover(i64)')                   # discover tests in module
        self.emit('declare i64 @test_filter(i64, i64)')                # filter tests by pattern
        self.emit('; spx Package Manager (Phase 34.8.2)')
        self.emit('declare i64 @spx_registry_fetch(i64, i64)')         # fetch(name, version)
        self.emit('declare i64 @spx_registry_search(i64)')             # search(query)
        self.emit('declare i64 @spx_resolve_deps(i64)')                # resolve_deps(manifest)
        self.emit('declare i64 @spx_semver_parse(i64)')                # parse semver string
        self.emit('declare i64 @spx_semver_satisfies(i64, i64)')       # satisfies(version, constraint)
        self.emit('declare i64 @spx_lock_read(i64)')                   # read spx.lock
        self.emit('declare void @spx_lock_write(i64, i64)')            # write spx.lock
        self.emit('declare i64 @spx_workspace_discover(i64)')          # discover workspace members
        self.emit('declare i64 @spx_features_resolve(i64, i64)')       # resolve features
        self.emit('declare i64 @spx_publish(i64, i64)')                # publish to registry
        self.emit('; sxc CLI Support (Phase 34.8.1)')
        self.emit('declare i64 @sxc_compile(i64, i64)')                # compile(input, output)
        self.emit('declare i64 @sxc_test(i64)')                        # run tests
        self.emit('declare i64 @sxc_watch(i64, i64)')                  # watch(dir, callback)
        self.emit('declare i64 @sxc_cross_compile(i64, i64, i64)')     # cross_compile(input, target, output)

        self.emit('; Phase 34.4.5 Graceful Shutdown')
        self.emit('declare i64 @signal_handler_register(i64, i64)')    # register(signal, handler)
        self.emit('declare void @signal_handler_unregister(i64)')      # unregister(signal)
        self.emit('declare i64 @shutdown_coordinator_new()')
        self.emit('declare void @shutdown_begin(i64)')                 # begin graceful shutdown
        self.emit('declare void @shutdown_wait(i64, i64)')             # wait with timeout
        self.emit('declare i64 @shutdown_is_shutting_down(i64)')
        self.emit('declare void @shutdown_drain_messages(i64)')        # drain in-flight messages
        self.emit('declare void @shutdown_checkpoint_all(i64)')        # checkpoint on shutdown

        self.emit('; Phase 34.5.3 Platform I/O Drivers')
        self.emit('declare i64 @epoll_driver_new()')                   # epoll for Linux
        self.emit('declare i64 @epoll_add(i64, i64, i64)')             # add(driver, fd, events)
        self.emit('declare i64 @epoll_remove(i64, i64)')               # remove(driver, fd)
        self.emit('declare i64 @epoll_wait(i64, i64)')                 # wait(driver, timeout)
        self.emit('declare void @epoll_driver_free(i64)')
        self.emit('declare i64 @iocp_driver_new()')                    # IOCP for Windows
        self.emit('declare i64 @iocp_associate(i64, i64)')             # associate(driver, handle)
        self.emit('declare i64 @iocp_post(i64, i64, i64)')             # post(driver, key, overlapped)
        self.emit('declare i64 @iocp_get(i64, i64)')                   # get(driver, timeout)
        self.emit('declare void @iocp_driver_free(i64)')
        self.emit('declare i64 @unified_io_new()')                     # platform-independent
        self.emit('declare i64 @unified_io_register(i64, i64, i64)')   # register(driver, fd, events)
        self.emit('declare i64 @unified_io_poll(i64, i64)')            # poll(driver, timeout)

        self.emit('; Phase 34.7.1 SWIM Gossip Protocol')
        self.emit('declare i64 @swim_new(i64, i64)')                   # new(bind_addr, port)
        self.emit('declare void @swim_add_seed(i64, i64, i64)')        # add_seed(swim, addr, port)
        self.emit('declare i64 @swim_start(i64)')                      # start gossip
        self.emit('declare void @swim_stop(i64)')                      # stop gossip
        self.emit('declare i64 @swim_ping(i64, i64)')                  # ping member
        self.emit('declare i64 @swim_ping_req(i64, i64, i64)')         # indirect ping
        self.emit('declare i64 @swim_suspect(i64, i64)')               # mark suspect
        self.emit('declare i64 @swim_alive_count(i64)')                # alive member count
        self.emit('declare i64 @swim_suspect_count(i64)')              # suspect count
        self.emit('declare i64 @swim_dead_count(i64)')                 # dead count
        self.emit('declare i64 @swim_gossip_round(i64)')               # run gossip round

        self.emit('; Phase 34.8.3 sxdoc Documentation Generator')
        self.emit('declare i64 @sxdoc_parse_module(i64)')              # parse doc comments
        self.emit('declare i64 @sxdoc_generate_html(i64, i64)')        # generate HTML
        self.emit('declare i64 @sxdoc_generate_json(i64)')             # generate JSON docs
        self.emit('declare i64 @sxdoc_build_xref(i64)')                # build cross-references
        self.emit('declare i64 @sxdoc_build_search_index(i64)')        # build search index
        self.emit('declare i64 @sxdoc_run_examples(i64)')              # run code examples

        self.emit('; Phase 34.8.4 sxlsp Language Server')
        self.emit('declare i64 @lsp_server_new()')                     # create LSP server
        self.emit('declare void @lsp_server_start(i64)')               # start listening
        self.emit('declare i64 @lsp_goto_definition(i64, i64, i64)')   # goto definition
        self.emit('declare i64 @lsp_find_references(i64, i64, i64)')   # find all references
        self.emit('declare i64 @lsp_rename_symbol(i64, i64, i64, i64)') # rename symbol
        self.emit('declare i64 @lsp_completion(i64, i64, i64)')        # code completion
        self.emit('declare i64 @lsp_signature_help(i64, i64, i64)')    # signature help
        self.emit('declare i64 @lsp_format_document(i64, i64)')        # format document

        self.emit('; Phase 34.8.6 cursus VM')
        self.emit('declare i64 @cursus_vm_new()')                      # create VM instance
        self.emit('declare i64 @cursus_load_bytecode(i64, i64)')       # load bytecode
        self.emit('declare i64 @cursus_execute(i64)')                  # execute program
        self.emit('declare i64 @cursus_jit_compile(i64, i64)')         # JIT compile function
        self.emit('declare i64 @cursus_serialize(i64, i64)')           # serialize bytecode
        self.emit('declare i64 @cursus_deserialize(i64)')              # deserialize bytecode

        self.emit('; Phase 34.11.3 Process Spawning')
        self.emit('declare i64 @process_spawn(i64, i64)')              # spawn(cmd, args)
        self.emit('declare i64 @process_spawn_with_env(i64, i64, i64)') # spawn with env
        self.emit('declare i64 @process_stdin(i64)')                   # get stdin pipe
        self.emit('declare i64 @process_stdout(i64)')                  # get stdout pipe
        self.emit('declare i64 @process_stderr(i64)')                  # get stderr pipe
        self.emit('declare i64 @process_wait(i64)')                    # wait for exit
        self.emit('declare i64 @process_kill(i64, i64)')               # kill(process, signal)
        self.emit('declare i64 @process_is_alive(i64)')                # check if running
        self.emit('declare i64 @process_exit_code(i64)')               # get exit code
        self.emit('declare void @process_set_env(i64, i64, i64)')      # set environment var

        self.emit('; Phase 34.11.4 Multi-threaded Async Executor')
        self.emit('declare i64 @mt_executor_new(i64)')                 # new(thread_count)
        self.emit('declare void @mt_executor_spawn(i64, i64)')         # spawn task
        self.emit('declare i64 @mt_executor_block_on(i64, i64)')       # block on future
        self.emit('declare void @mt_executor_shutdown(i64)')           # shutdown
        self.emit('declare i64 @mt_executor_thread_count(i64)')        # get thread count
        self.emit('declare i64 @"block_on"(i64)')                      # simple block_on(future)
        self.emit('declare i64 @mt_task_queue_new()')                  # thread-safe queue
        self.emit('declare void @mt_task_queue_push(i64, i64)')        # push task
        self.emit('declare i64 @mt_task_queue_pop(i64)')               # pop task
        self.emit('declare i64 @mt_task_queue_steal(i64)')             # steal from queue

        self.emit('; Phase 34.11.13 ABA Problem Prevention')
        self.emit('declare i64 @hazard_ptr_new()')                     # create hazard pointer
        self.emit('declare void @hazard_ptr_protect(i64, i64)')        # protect(hp, ptr)
        self.emit('declare void @hazard_ptr_release(i64)')             # release protection
        self.emit('declare i64 @hazard_ptr_is_protected(i64)')         # check if protected
        self.emit('declare i64 @epoch_new()')                          # epoch-based reclamation
        self.emit('declare void @epoch_pin(i64)')                      # pin current epoch
        self.emit('declare void @epoch_unpin(i64)')                    # unpin epoch
        self.emit('declare void @epoch_defer_free(i64, i64)')          # defer free to safe epoch
        self.emit('declare void @epoch_collect(i64)')                  # collect garbage

        self.emit('; Incremental compilation')
        self.emit('declare i64 @incremental_cache_get(i64)')           # get cached artifact
        self.emit('declare void @incremental_cache_put(i64, i64)')     # cache artifact
        self.emit('declare i64 @incremental_needs_rebuild(i64)')       # check if rebuild needed
        self.emit('; Generator/Iterator intrinsics')
        self.emit('declare i64 @generator_yield(i64)')
        self.emit('declare i64 @generator_new(i64)')
        self.emit('declare i64 @generator_next(i64)')
        self.emit('declare i64 @generator_has_next(i64)')
        self.emit('; Stream<T> type (lazy iterator/generator)')
        self.emit('declare i64 @stream_new(i64)')               # create stream from generator fn
        self.emit('declare i64 @stream_next(i64)')              # get next value (Option<T>)
        self.emit('declare i64 @stream_collect(i64)')           # collect into Vec<T>
        self.emit('declare i64 @stream_map(i64, i64)')          # stream.map(fn)
        self.emit('declare i64 @stream_filter(i64, i64)')       # stream.filter(predicate)
        self.emit('declare i64 @stream_take(i64, i64)')         # stream.take(n)
        self.emit('declare i64 @stream_skip(i64, i64)')         # stream.skip(n)
        self.emit('declare i64 @stream_fold(i64, i64, i64)')    # stream.fold(init, fn)
        self.emit('declare i64 @stream_chain(i64, i64)')        # stream.chain(other)
        self.emit('declare i64 @stream_zip(i64, i64)')          # stream.zip(other)
        self.emit('; Iterator protocol')
        self.emit('declare i64 @vec_iter(i64)')                 # Vec.iter() -> Iterator
        self.emit('declare i64 @iter_next(i64)')                # Iterator.next() -> Option<T>
        self.emit('declare i64 @iterator_new(i64)')             # create iterator
        self.emit('declare i64 @iterator_next(i64)')            # get next (Option<T>)
        self.emit('; Future/async intrinsics')
        self.emit('declare i64 @future_ready(i64)')             # Create completed future
        self.emit('declare i64 @future_poll(i64)')              # Poll future (simplified)
        self.emit('declare i64 @future_pending()')              # Create pending future
        self.emit('declare i64 @iterator_size_hint(i64)')       # (lower, upper) bounds
        self.emit('declare i64 @iterator_count(i64)')           # consume and count
        self.emit('declare i64 @iterator_last(i64)')            # consume and get last
        self.emit('declare i64 @iterator_nth(i64, i64)')        # get nth element
        self.emit('declare i64 @iterator_enumerate(i64)')       # add indices
        self.emit('; Process execution')
        self.emit('declare i64 @intrinsic_process_run(ptr)')
        self.emit('declare ptr @intrinsic_process_output(ptr)')
        self.emit('; File system intrinsics')
        self.emit('declare ptr @intrinsic_getenv(ptr)')
        self.emit('declare i64 @intrinsic_file_exists(ptr)')
        self.emit('declare i64 @intrinsic_is_file(ptr)')
        self.emit('declare i64 @intrinsic_is_directory(ptr)')
        self.emit('declare i64 @intrinsic_file_size(ptr)')
        self.emit('declare i64 @intrinsic_file_mtime(ptr)')
        self.emit('declare i64 @intrinsic_remove_path(ptr)')
        self.emit('declare i64 @intrinsic_mkdir_p(ptr)')
        self.emit('declare ptr @intrinsic_get_cwd()')
        self.emit('declare i64 @intrinsic_set_cwd(ptr)')
        self.emit('declare ptr @intrinsic_list_dir(ptr)')
        self.emit('declare ptr @intrinsic_path_join(ptr, ptr)')
        self.emit('declare ptr @intrinsic_path_dirname(ptr)')
        self.emit('declare ptr @intrinsic_path_basename(ptr)')
        self.emit('declare ptr @intrinsic_path_extension(ptr)')
        self.emit('; Phase 7: Actor Runtime')
        self.emit('declare i64 @intrinsic_get_num_cpus()')
        self.emit('; Threading')
        self.emit('declare ptr @intrinsic_thread_spawn(ptr, ptr)')
        self.emit('declare void @intrinsic_thread_join(ptr)')
        self.emit('declare i64 @intrinsic_thread_id(ptr)')
        self.emit('declare void @intrinsic_sleep_ms(i64)')
        self.emit('declare void @intrinsic_thread_yield()')
        self.emit('; Mutex')
        self.emit('declare ptr @intrinsic_mutex_new()')
        self.emit('declare void @intrinsic_mutex_lock(ptr)')
        self.emit('declare void @intrinsic_mutex_unlock(ptr)')
        self.emit('declare void @intrinsic_mutex_free(ptr)')
        self.emit('; Condition variable')
        self.emit('declare ptr @intrinsic_condvar_new()')
        self.emit('declare void @intrinsic_condvar_wait(ptr, ptr)')
        self.emit('declare void @intrinsic_condvar_signal(ptr)')
        self.emit('declare void @intrinsic_condvar_broadcast(ptr)')
        self.emit('declare void @intrinsic_condvar_free(ptr)')
        self.emit('; Atomics')
        self.emit('declare i64 @intrinsic_atomic_load(ptr)')
        self.emit('declare void @intrinsic_atomic_store(ptr, i64)')
        self.emit('declare i64 @intrinsic_atomic_add(ptr, i64)')
        self.emit('declare i64 @intrinsic_atomic_sub(ptr, i64)')
        self.emit('declare i1 @intrinsic_atomic_cas(ptr, i64, i64)')
        self.emit('declare ptr @intrinsic_atomic_load_ptr(ptr)')
        self.emit('declare void @intrinsic_atomic_store_ptr(ptr, ptr)')
        self.emit('declare i1 @intrinsic_atomic_cas_ptr(ptr, ptr, ptr)')
        self.emit('; Mailbox')
        self.emit('declare ptr @intrinsic_mailbox_new()')
        self.emit('declare void @intrinsic_mailbox_send(ptr, ptr)')
        self.emit('declare ptr @intrinsic_mailbox_recv(ptr)')
        self.emit('declare i1 @intrinsic_mailbox_empty(ptr)')
        self.emit('declare i64 @intrinsic_mailbox_len(ptr)')
        self.emit('declare void @intrinsic_mailbox_free(ptr)')
        self.emit('; Actor')
        self.emit('declare ptr @intrinsic_actor_spawn(ptr, ptr)')
        self.emit('declare void @intrinsic_actor_send(ptr, ptr)')
        self.emit('declare ptr @intrinsic_actor_state(ptr)')
        self.emit('declare void @intrinsic_actor_set_state(ptr, ptr)')
        self.emit('declare ptr @intrinsic_actor_mailbox(ptr)')
        self.emit('declare i64 @intrinsic_actor_id(ptr)')
        self.emit('declare ptr @intrinsic_actor_ask(ptr, ptr)')
        self.emit('; Phase 4.7: Actor Checkpointing')
        self.emit('declare i64 @actor_checkpoint_save(i64, ptr, i64)')
        self.emit('declare i64 @actor_checkpoint_load(ptr, i64)')
        self.emit('declare i64 @actor_checkpoint_get_id(ptr)')
        self.emit('declare i64 @actor_checkpoint_exists(ptr)')
        self.emit('declare i64 @actor_checkpoint_delete(ptr)')
        self.emit('declare i64 @actor_spawn_from_checkpoint(ptr, ptr, i64)')
        self.emit('; Phase 8: Async Runtime')
        self.emit('; I/O Driver')
        self.emit('declare ptr @intrinsic_io_driver_new()')
        self.emit('declare void @intrinsic_io_driver_init()')
        self.emit('declare void @intrinsic_io_driver_register_read(ptr, i64, ptr)')
        self.emit('declare void @intrinsic_io_driver_register_write(ptr, i64, ptr)')
        self.emit('declare void @intrinsic_io_driver_unregister(ptr, i64)')
        self.emit('declare i64 @intrinsic_io_driver_poll(ptr, i64)')
        self.emit('declare void @intrinsic_io_driver_free(ptr)')
        self.emit('declare i1 @intrinsic_set_nonblocking(i64)')
        self.emit('; Timer wheel')
        self.emit('declare ptr @intrinsic_timer_wheel_new()')
        self.emit('declare void @intrinsic_timer_wheel_init()')
        self.emit('declare void @intrinsic_timer_register(ptr, i64, ptr)')
        self.emit('declare i64 @intrinsic_timer_check(ptr)')
        self.emit('declare i64 @intrinsic_timer_next_deadline(ptr)')
        self.emit('declare void @intrinsic_timer_wheel_free(ptr)')
        self.emit('; Executor')
        self.emit('declare ptr @intrinsic_executor_new()')
        self.emit('declare void @intrinsic_executor_init()')
        self.emit('declare i64 @intrinsic_executor_spawn(ptr, ptr, ptr)')
        self.emit('declare void @intrinsic_executor_wake(ptr, i64)')
        self.emit('declare void @intrinsic_executor_run(ptr)')
        self.emit('declare void @intrinsic_executor_stop(ptr)')
        self.emit('declare void @intrinsic_executor_free(ptr)')
        self.emit('declare i64 @intrinsic_now_ms()')
        self.emit('; Phase 9: Networking')
        self.emit('declare i64 @intrinsic_socket_create(i64, i64)')
        self.emit('declare void @intrinsic_socket_set_nonblocking(i64)')
        self.emit('declare void @intrinsic_socket_set_reuseaddr(i64)')
        self.emit('declare i64 @intrinsic_socket_bind(i64, i64, i64)')
        self.emit('declare i64 @intrinsic_socket_listen(i64, i64)')
        self.emit('declare i64 @intrinsic_socket_accept(i64, ptr, ptr)')
        self.emit('declare i64 @intrinsic_socket_connect(i64, i64, i64)')
        self.emit('declare i64 @intrinsic_socket_read(i64, ptr, i64)')
        self.emit('declare i64 @intrinsic_socket_write(i64, ptr, i64)')
        self.emit('declare void @intrinsic_socket_close(i64)')
        self.emit('declare i64 @intrinsic_socket_get_error(i64)')
        self.emit('declare i64 @intrinsic_socket_sendto(i64, ptr, i64, i64, i64)')
        self.emit('declare i64 @intrinsic_socket_recvfrom(i64, ptr, i64, ptr, ptr)')
        self.emit('declare i64 @intrinsic_dns_resolve(ptr, ptr)')
        self.emit('declare ptr @intrinsic_ip_to_string(i64)')
        self.emit('declare ptr @intrinsic_string_to_ip(ptr)')
        self.emit('declare i64 @intrinsic_get_errno()')
        self.emit('; Phase 20: Toolchain Support')
        self.emit('declare ptr @intrinsic_read_line()')
        self.emit('declare i1 @intrinsic_is_tty()')
        self.emit('declare i1 @intrinsic_stdin_has_data()')
        self.emit('declare i64 @intrinsic_string_hash(ptr)')
        self.emit('declare i64 @intrinsic_string_find(ptr, ptr, i64)')
        self.emit('declare ptr @intrinsic_string_trim(ptr)')
        self.emit('declare ptr @intrinsic_string_split(ptr, ptr)')
        self.emit('declare i1 @intrinsic_string_starts_with(ptr, ptr)')
        self.emit('declare i1 @intrinsic_string_ends_with(ptr, ptr)')
        self.emit('declare i1 @intrinsic_string_contains(ptr, ptr)')
        self.emit('declare ptr @intrinsic_string_replace(ptr, ptr, ptr)')
        self.emit('declare i64 @intrinsic_copy_file(ptr, ptr)')
        self.emit('declare ptr @intrinsic_get_home_dir()')
        self.emit('; Phase 22: Async Runtime - Future trait and Poll enum')
        self.emit('; Poll<T> enum: Ready(T) = value << 1 | 1, Pending = 0')
        self.emit('; Future trait: poll(self, ctx: *Context) -> Poll<T>')
        self.emit('; (declarations moved to iterator section above to avoid duplicates)')
        self.emit('; Waker implementation')
        self.emit('declare i64 @waker_new(i64, i64)')          # waker_new(wake_fn, data)
        self.emit('declare void @waker_wake(i64)')              # waker_wake(waker)
        self.emit('declare void @waker_wake_by_ref(i64)')       # waker_wake_by_ref(waker)
        self.emit('declare i64 @waker_clone(i64)')              # waker_clone(waker)
        self.emit('declare void @waker_drop(i64)')              # waker_drop(waker)
        self.emit('; Context implementation')
        self.emit('declare i64 @context_new(i64)')              # context_new(waker) -> ctx
        self.emit('declare i64 @context_waker(i64)')            # context_waker(ctx) -> waker
        self.emit('declare void @context_free(i64)')            # context_free(ctx)
        self.emit('; RawWaker vtable for custom wakers')
        self.emit('declare i64 @raw_waker_new(i64, i64)')       # raw_waker_new(data, vtable)
        self.emit('declare void @executor_run(i64)')
        self.emit('declare i64 @executor_spawn(i64)')
        self.emit('; Phase 22.2: Async Combinators')
        self.emit('declare i64 @async_join(i64, i64)')
        self.emit('declare i64 @async_join3(i64, i64, i64)')
        self.emit('declare i64 @async_join_all(i64)')               # join_all(vec_of_futures)
        self.emit('declare i64 @join_result1(i64)')
        self.emit('declare i64 @join_result2(i64)')
        self.emit('declare i64 @join_result3(i64)')
        self.emit('declare i64 @async_select(i64, i64)')
        self.emit('declare i64 @async_select3(i64, i64, i64)')
        self.emit('declare i64 @select_result(i64)')
        self.emit('declare i64 @select_which(i64)')
        self.emit('declare i64 @async_timeout(i64, i64)')
        self.emit('declare i64 @timeout_result(i64)')
        self.emit('declare i64 @timeout_expired(i64)')
        self.emit('declare i64 @time_now_ms()')
        self.emit('; Proper cancellation')
        self.emit('declare i64 @cancel_token_new()')
        self.emit('declare void @cancel_token_cancel(i64)')
        self.emit('declare i64 @cancel_token_is_cancelled(i64)')
        self.emit('declare i64 @with_cancel(i64, i64)')             # with_cancel(future, cancel_token)
        self.emit('declare void @future_cancel(i64)')               # request cancellation
        self.emit('; Phase 22.3: Pin<T>')
        self.emit('declare i64 @pin_new(i64, i64)')
        self.emit('declare i64 @pin_new_uninit(i64)')
        self.emit('declare i64 @pin_get(i64)')
        self.emit('declare i64 @pin_get_mut(i64)')
        self.emit('declare i64 @pin_is_pinned(i64)')
        self.emit('declare void @pin_ref(i64)')
        self.emit('declare void @pin_unref(i64)')
        self.emit('declare void @pin_set_self_ref(i64, i64)')
        self.emit('declare i64 @pin_check_self_ref(i64, i64)')
        self.emit('; Phase 22.5: Function pointer calls')
        self.emit('declare i64 @intrinsic_call0(i64)')
        self.emit('declare i64 @intrinsic_call1(i64, i64)')
        self.emit('declare i64 @intrinsic_call2(i64, i64, i64)')
        self.emit('declare i64 @intrinsic_call3(i64, i64, i64, i64)')
        self.emit('; Phase 22.6: Structured Concurrency')
        self.emit('declare i64 @scope_new()')
        self.emit('declare i64 @scope_spawn(i64, i64)')
        self.emit('declare i64 @scope_poll(i64)')
        self.emit('declare i64 @scope_join(i64)')
        self.emit('declare i64 @scope_get_result(i64, i64)')
        self.emit('declare void @scope_cancel(i64)')
        self.emit('declare i64 @scope_count(i64)')
        self.emit('declare i64 @scope_completed(i64)')
        self.emit('declare void @scope_free(i64)')
        self.emit('declare i64 @nursery_run(i64, i64)')
        self.emit('; Phase 23.4: Actor Error Handling')
        self.emit('declare i64 @actor_get_status(i64)')
        self.emit('declare i64 @actor_get_exit_reason(i64)')
        self.emit('declare i64 @actor_get_error_code(i64)')
        self.emit('declare void @actor_set_error(i64, i64, i64)')
        self.emit('declare void @actor_stop(i64)')
        self.emit('declare void @actor_kill(i64)')
        self.emit('declare void @actor_crash(i64, i64, i64)')
        self.emit('declare void @actor_set_on_error(i64, i64)')
        self.emit('declare void @actor_set_on_exit(i64, i64)')
        self.emit('declare void @actor_set_supervisor(i64, i64)')
        self.emit('declare i64 @actor_get_supervisor(i64)')
        self.emit('declare i64 @actor_get_restart_count(i64)')
        self.emit('declare void @actor_increment_restart(i64)')
        self.emit('declare i64 @actor_is_alive(i64)')
        self.emit('; Circuit Breaker')
        self.emit('declare i64 @circuit_breaker_new(i64, i64, i64)')
        self.emit('declare i64 @circuit_breaker_allow(i64)')
        self.emit('declare void @circuit_breaker_success(i64)')
        self.emit('declare void @circuit_breaker_failure(i64)')
        self.emit('declare i64 @circuit_breaker_state(i64)')
        self.emit('declare void @circuit_breaker_reset(i64)')
        self.emit('; Retry Policy')
        self.emit('declare i64 @retry_policy_new(i64, i64, i64, i64)')
        self.emit('declare void @retry_policy_set_jitter(i64, i64)')
        self.emit('declare i64 @retry_policy_should_retry(i64)')
        self.emit('declare i64 @retry_policy_next_delay(i64)')
        self.emit('declare void @retry_policy_reset(i64)')
        self.emit('declare i64 @retry_policy_count(i64)')
        self.emit('; Phase 23.5: Actor Linking and Monitoring')
        self.emit('declare i64 @actor_link(i64, i64)')
        self.emit('declare void @actor_unlink(i64, i64)')
        self.emit('declare i64 @actor_monitor(i64, i64)')
        self.emit('declare void @actor_demonitor(i64)')
        self.emit('declare void @actor_propagate_exit(i64, i64)')
        self.emit('declare i64 @actor_is_linked(i64, i64)')
        self.emit('declare i64 @actor_spawn_link(i64, i64, i64)')
        self.emit('declare i64 @actor_get_links_count(i64)')
        self.emit('declare i64 @actor_send_down(i64, i64, i64)')

        # Phase 23.1: Supervision Trees
        self.emit('; Phase 23.1: Supervision Trees')
        self.emit('declare i64 @supervisor_new(i64, i64, i64)')
        self.emit('declare i64 @supervisor_add_child(i64, i64, i64, i64, i64)')
        self.emit('declare i64 @supervisor_start(i64)')
        self.emit('declare void @supervisor_stop(i64)')
        self.emit('declare i64 @supervisor_handle_exit(i64, i64, i64)')
        self.emit('declare i64 @supervisor_child_count(i64)')
        self.emit('declare i64 @supervisor_child_status(i64, i64)')
        self.emit('declare i64 @supervisor_child_handle(i64, i64)')
        self.emit('declare void @supervisor_free(i64)')
        self.emit('declare i64 @strategy_one_for_one()')
        self.emit('declare i64 @strategy_one_for_all()')
        self.emit('declare i64 @strategy_rest_for_one()')
        self.emit('declare i64 @child_permanent()')
        self.emit('declare i64 @child_temporary()')
        self.emit('declare i64 @child_transient()')

        # Phase 23.2: Work-Stealing Scheduler
        self.emit('; Phase 23.2: Work-Stealing Scheduler')
        self.emit('declare i64 @scheduler_new(i64)')
        self.emit('declare i64 @scheduler_start(i64)')
        self.emit('declare i64 @scheduler_submit(i64, i64, i64)')
        self.emit('declare i64 @scheduler_submit_local(i64, i64, i64, i64)')
        self.emit('declare void @scheduler_stop(i64)')
        self.emit('declare void @scheduler_free(i64)')
        self.emit('declare i64 @scheduler_worker_count(i64)')
        self.emit('declare i64 @scheduler_queue_size(i64)')
        self.emit('declare i64 @scheduler_worker_idle(i64, i64)')
        self.emit('; Work-stealing algorithm')
        self.emit('declare i64 @scheduler_steal_from(i64, i64)')     # steal_from(scheduler, victim_worker)
        self.emit('declare i64 @scheduler_try_steal(i64)')           # try_steal from any worker
        self.emit('declare i64 @scheduler_local_queue_size(i64, i64)') # local_queue_size(sched, worker)
        self.emit('; Load balancing')
        self.emit('declare void @scheduler_rebalance(i64)')          # trigger load rebalancing
        self.emit('declare i64 @scheduler_get_load(i64, i64)')       # get_load(sched, worker)
        self.emit('; Thread parking/unparking')
        self.emit('declare void @worker_park(i64)')                  # park current worker
        self.emit('declare void @worker_unpark(i64, i64)')           # unpark(sched, worker_id)
        self.emit('declare i64 @worker_is_parked(i64, i64)')         # is_parked(sched, worker_id)

        # Phase 23.3: Lock-Free Mailbox (Michael-Scott Queue)
        self.emit('; Phase 23.3: Lock-Free Mailbox - Michael-Scott Queue Implementation')
        self.emit('declare i64 @mailbox_new(i64)')
        self.emit('declare i64 @mailbox_send(i64, i64)')
        self.emit('declare i64 @mailbox_recv(i64)')
        self.emit('declare i64 @mailbox_try_recv(i64)')
        self.emit('declare i64 @mailbox_size(i64)')
        self.emit('declare i64 @mailbox_empty(i64)')
        self.emit('declare i64 @mailbox_full(i64)')
        self.emit('declare void @mailbox_close(i64)')
        self.emit('declare i64 @mailbox_is_closed(i64)')
        self.emit('declare void @mailbox_free(i64)')
        self.emit('; CAS (Compare-And-Swap) operations for lock-free')
        self.emit('declare i64 @atomic_cas_i64(ptr, i64, i64)')     # cas(ptr, expected, new) -> old
        self.emit('declare i64 @atomic_load_i64(ptr)')              # atomic load
        self.emit('declare void @atomic_store_i64(ptr, i64)')       # atomic store
        self.emit('declare i64 @atomic_fetch_add(ptr, i64)')        # fetch_add
        self.emit('declare i64 @atomic_fetch_sub(ptr, i64)')        # fetch_sub
        self.emit('; Memory ordering fences')
        self.emit('declare void @memory_fence_acquire()')
        self.emit('declare void @memory_fence_release()')
        self.emit('declare void @memory_fence_acq_rel()')
        self.emit('declare void @memory_fence_seq_cst()')

        # Phase 23.6: Actor Discovery and Registry
        self.emit('; Phase 23.6: Actor Discovery and Registry')
        self.emit('declare i64 @registry_register(i64, i64)')
        self.emit('declare void @registry_unregister(i64)')
        self.emit('declare i64 @registry_lookup(i64)')
        self.emit('declare i64 @registry_count()')
        self.emit('declare i64 @registry_set_metadata(i64, i64)')
        self.emit('declare i64 @registry_get_metadata(i64)')

        # Phase 23.7: Backpressure and Flow Control
        self.emit('; Phase 23.7: Backpressure and Flow Control')
        self.emit('declare i64 @flow_controller_new(i64, i64, i64)')
        self.emit('declare i64 @flow_check(i64)')
        self.emit('declare i64 @flow_acquire(i64)')
        self.emit('declare void @flow_release(i64)')
        self.emit('declare i64 @flow_is_signaling(i64)')
        self.emit('declare i64 @flow_current(i64)')
        self.emit('declare i64 @flow_high_watermark(i64)')
        self.emit('declare i64 @flow_low_watermark(i64)')
        self.emit('declare void @flow_reset(i64)')
        self.emit('declare void @flow_free(i64)')
        self.emit('declare i64 @flow_mode_drop()')
        self.emit('declare i64 @flow_mode_block()')
        self.emit('declare i64 @flow_mode_signal()')
        self.emit('; Phase 24.1: TLS/SSL Support - Full Implementation')
        self.emit('declare i64 @tls_context_new_client()')
        self.emit('declare i64 @tls_context_new_server()')
        self.emit('declare i64 @tls_context_load_cert(i64, i64)')
        self.emit('declare i64 @tls_context_load_key(i64, i64)')
        self.emit('declare i64 @tls_context_load_ca(i64, i64)')
        self.emit('declare i64 @tls_context_use_system_ca(i64)')
        self.emit('declare void @tls_context_set_verify(i64, i64)')
        self.emit('declare void @tls_context_free(i64)')
        self.emit('declare i64 @tls_connect(i64, i64, i64)')
        self.emit('declare i64 @tls_accept(i64, i64)')
        self.emit('declare i64 @tls_read(i64, i64, i64)')
        self.emit('declare i64 @tls_write(i64, i64, i64)')
        self.emit('declare void @tls_shutdown(i64)')
        self.emit('declare void @tls_close(i64)')
        self.emit('declare i64 @tls_peer_cert_subject(i64)')
        self.emit('declare i64 @tls_peer_verified(i64)')
        self.emit('declare i64 @tls_version(i64)')
        self.emit('declare i64 @tls_cipher(i64)')
        self.emit('declare i64 @tls_error_string()')
        self.emit('declare i64 @tls_get_fd(i64)')
        self.emit('; Full TLS handshake')
        self.emit('declare i64 @tls_do_handshake(i64)')              # explicit handshake
        self.emit('declare i64 @tls_handshake_state(i64)')           # get handshake state
        self.emit('declare i64 @tls_is_handshake_done(i64)')
        self.emit('; Certificate validation')
        self.emit('declare i64 @tls_cert_verify_result(i64)')        # get verification result
        self.emit('declare i64 @tls_cert_chain_depth(i64)')          # chain depth
        self.emit('declare i64 @tls_cert_at_depth(i64, i64)')        # get cert at depth
        self.emit('declare void @tls_set_verify_callback(i64, i64)') # custom verify
        self.emit('; Certificate pinning')
        self.emit('declare void @tls_pin_pubkey(i64, i64)')          # pin_pubkey(ctx, hash)
        self.emit('declare void @tls_pin_cert(i64, i64)')            # pin_cert(ctx, hash)
        self.emit('declare i64 @tls_pinned_match(i64)')              # check pin match
        self.emit('; ALPN/SNI negotiation')
        self.emit('declare void @tls_set_alpn(i64, i64)')            # set_alpn(ctx, protocols)
        self.emit('declare i64 @tls_alpn_selected(i64)')             # get selected protocol
        self.emit('declare void @tls_set_sni(i64, i64)')             # set_sni(ctx, hostname)
        self.emit('declare i64 @tls_sni_servername(i64)')            # get SNI servername
        self.emit('; Phase 24.2: HTTP Client - Full Implementation')
        self.emit('declare i64 @http_request_new(i64, i64)')
        self.emit('declare void @http_request_header(i64, i64, i64)')
        self.emit('declare void @http_request_body(i64, i64)')
        self.emit('declare i64 @http_request_send(i64)')
        self.emit('declare void @http_request_free(i64)')
        self.emit('declare i64 @http_response_status(i64)')
        self.emit('declare i64 @http_response_status_text(i64)')
        self.emit('declare i64 @http_response_header(i64, i64)')
        self.emit('declare i64 @http_response_body(i64)')
        self.emit('declare i64 @http_response_body_len(i64)')
        self.emit('declare void @http_response_free(i64)')
        self.emit('declare i64 @http_get(i64)')
        self.emit('declare i64 @http_post(i64, i64)')
        self.emit('; Full URL parsing')
        self.emit('declare i64 @url_parse(i64)')                     # parse(url_str) -> URL
        self.emit('declare i64 @url_scheme(i64)')                    # url_scheme(url) -> string
        self.emit('declare i64 @url_host(i64)')                      # url_host(url) -> string
        self.emit('declare i64 @url_port(i64)')                      # url_port(url) -> i64
        self.emit('declare i64 @url_path(i64)')                      # url_path(url) -> string
        self.emit('declare i64 @url_query(i64)')                     # url_query(url) -> string
        self.emit('; Header parsing')
        self.emit('declare i64 @http_headers_get_all(i64, i64)')     # get all headers with name
        self.emit('declare i64 @http_headers_iter(i64)')             # iterate headers
        self.emit('; Body streaming (chunked transfer)')
        self.emit('declare i64 @http_body_stream_new(i64)')
        self.emit('declare i64 @http_body_stream_read(i64, i64)')    # read chunk
        self.emit('declare i64 @http_body_stream_done(i64)')
        self.emit('; Redirect following')
        self.emit('declare void @http_request_follow_redirects(i64, i64)')  # set redirect policy
        self.emit('declare i64 @http_response_redirect_url(i64)')    # get redirect URL if any
        self.emit('; Connection pooling')
        self.emit('declare i64 @http_client_new()')
        self.emit('declare void @http_client_set_pool_size(i64, i64)')
        self.emit('declare i64 @http_client_request(i64, i64)')
        self.emit('declare void @http_client_free(i64)')
        self.emit('; Timeout handling')
        self.emit('declare void @http_request_timeout(i64, i64)')    # set timeout in ms
        self.emit('; Phase 24.3: HTTP Server - Full Implementation')
        self.emit('declare i64 @http_server_new(i64)')
        self.emit('declare i64 @http_server_tls(i64, i64, i64)')
        self.emit('declare void @http_server_route(i64, i64, i64, i64)')
        self.emit('declare i64 @http_server_response_new()')
        self.emit('declare void @http_server_response_status(i64, i64, i64)')
        self.emit('declare void @http_server_response_header(i64, i64, i64)')
        self.emit('declare void @http_server_response_body(i64, i64)')
        self.emit('declare i64 @http_server_bind(i64)')
        self.emit('declare i64 @http_server_accept_one(i64)')
        self.emit('declare i64 @http_server_run(i64, i64)')
        self.emit('declare void @http_server_stop(i64)')
        self.emit('declare void @http_server_close(i64)')
        self.emit('declare i64 @http_server_port(i64)')
        self.emit('declare i64 @http_server_request_method(i64)')
        self.emit('declare i64 @http_server_request_path(i64)')
        self.emit('declare i64 @http_server_request_header(i64, i64)')
        self.emit('declare i64 @http_server_request_body(i64)')
        self.emit('; Full router/handler registration')
        self.emit('declare void @http_server_route_regex(i64, i64, i64, i64)')
        self.emit('declare void @http_server_middleware(i64, i64)')
        self.emit('; Request multiplexing')
        self.emit('declare i64 @http_server_multiplex_accept(i64)')
        self.emit('declare void @http_server_set_max_connections(i64, i64)')
        self.emit('; Response streaming')
        self.emit('declare i64 @http_response_stream_new(i64)')
        self.emit('declare void @http_response_stream_write(i64, i64)')
        self.emit('declare void @http_response_stream_end(i64)')
        self.emit('; Keep-alive handling')
        self.emit('declare void @http_server_set_keepalive(i64, i64)')
        self.emit('declare void @http_server_set_keepalive_timeout(i64, i64)')
        self.emit('; Connection limits')
        self.emit('declare void @http_server_set_max_body_size(i64, i64)')
        self.emit('declare void @http_server_set_header_timeout(i64, i64)')
        self.emit('; Phase 24.4: WebSocket Support')
        self.emit('declare i64 @ws_connect(i64)')
        self.emit('declare i64 @ws_accept(i64, i64)')
        self.emit('declare i64 @ws_send_text(i64, i64)')
        self.emit('declare i64 @ws_send_binary(i64, i64, i64)')
        self.emit('declare i64 @ws_recv(i64)')
        self.emit('declare i64 @ws_ping(i64)')
        self.emit('declare i64 @ws_pong(i64)')
        self.emit('declare void @ws_close(i64)')
        self.emit('declare i64 @ws_is_connected(i64)')
        self.emit('declare i64 @ws_opcode_text()')
        self.emit('declare i64 @ws_opcode_binary()')
        self.emit('declare i64 @ws_opcode_close()')
        self.emit('declare i64 @ws_opcode_ping()')
        self.emit('declare i64 @ws_opcode_pong()')
        self.emit('; Phase 25: Distribution & Clustering')
        self.emit('; 25.1 Cluster Membership')
        self.emit('declare i64 @cluster_new(i64, i64, i64)')
        self.emit('declare i64 @cluster_add_seed(i64, i64, i64)')
        self.emit('declare i64 @cluster_start(i64)')
        self.emit('declare i64 @cluster_stop(i64)')
        self.emit('declare i64 @cluster_member_count(i64)')
        self.emit('declare i64 @cluster_member_state(i64, i64)')
        self.emit('declare i64 @cluster_is_alive(i64, i64)')
        self.emit('declare i64 @cluster_self_id(i64)')
        self.emit('declare void @cluster_close(i64)')
        self.emit('; 25.2 DHT')
        self.emit('declare i64 @dht_new(i64)')
        self.emit('declare i64 @dht_rebuild_ring(i64)')
        self.emit('declare i64 @dht_find_node(i64, i64)')
        self.emit('declare i64 @dht_put(i64, i64, i64)')
        self.emit('declare i64 @dht_get(i64, i64)')
        self.emit('declare i64 @dht_delete(i64, i64)')
        self.emit('declare void @dht_close(i64)')
        self.emit('; 25.3 Migration')
        self.emit('declare i64 @migration_serialize_actor(i64)')
        self.emit('declare i64 @migration_deserialize_actor(i64)')
        self.emit('declare i64 @migration_new(i64, i64)')
        self.emit('declare i64 @migration_start(i64, i64)')
        self.emit('declare i64 @migration_status(i64)')
        self.emit('declare i64 @migration_rollback(i64)')
        self.emit('declare void @migration_close(i64)')
        self.emit('; 25.4 Code Store')
        self.emit('declare i64 @code_store_new()')
        self.emit('declare i64 @code_store_put(i64, i64)')
        self.emit('declare i64 @code_store_get(i64, i64)')
        self.emit('declare i64 @code_store_put_ast(i64, i64, i64)')
        self.emit('declare i64 @code_store_get_ast(i64, i64)')
        self.emit('declare void @code_store_close(i64)')
        self.emit('; 25.5 Partition Detection')
        self.emit('declare i64 @partition_detector_new(i64)')
        self.emit('declare i64 @partition_set_quorum(i64, i64)')
        self.emit('declare i64 @partition_check(i64)')
        self.emit('declare i64 @partition_has_quorum(i64)')
        self.emit('declare i64 @partition_reachable_count(i64)')
        self.emit('declare void @partition_detector_close(i64)')
        self.emit('; 25.6 Vector Clocks')
        self.emit('declare i64 @vclock_new()')
        self.emit('declare i64 @vclock_increment(i64, i64)')
        self.emit('declare i64 @vclock_get(i64, i64)')
        self.emit('declare i64 @vclock_compare(i64, i64)')
        self.emit('declare i64 @vclock_merge(i64, i64)')
        self.emit('declare void @vclock_close(i64)')
        self.emit('; 25.7 Node Authentication')
        self.emit('declare i64 @node_auth_new()')
        self.emit('declare i64 @node_auth_load_cert(i64, i64)')
        self.emit('declare i64 @node_auth_load_key(i64, i64)')
        self.emit('declare i64 @node_auth_load_ca(i64, i64)')
        self.emit('declare i64 @node_auth_set_secret(i64, i64)')
        self.emit('declare i64 @node_auth_connect(i64, i64, i64)')
        self.emit('declare i64 @node_auth_accept(i64, i64)')
        self.emit('declare void @node_auth_close_conn(i64)')
        self.emit('declare void @node_auth_close(i64)')
        self.emit('; Phase 26: Semantic Memory')
        self.emit('; 26.1 Embeddings')
        self.emit('declare i64 @embedding_model_new(i64)')
        self.emit('declare i64 @embedding_model_load(i64, i64)')
        self.emit('declare i64 @embedding_embed(i64, i64)')
        self.emit('declare i64 @embedding_batch_embed(i64, i64)')
        self.emit('declare i64 @embedding_dim(i64)')
        self.emit('declare double @embedding_get(i64, i64)')
        self.emit('declare double @embedding_cosine_similarity(i64, i64)')
        self.emit('declare void @embedding_free(i64)')
        self.emit('declare void @embedding_model_close(i64)')
        self.emit('; 26.2 HNSW Index')
        self.emit('declare i64 @hnsw_new()')
        self.emit('declare i64 @hnsw_insert(i64, i64, i64)')
        self.emit('declare i64 @hnsw_search(i64, i64, i64)')
        self.emit('declare i64 @hnsw_count(i64)')
        self.emit('declare i64 @hnsw_get_data(i64, i64)')
        self.emit('declare void @hnsw_close(i64)')
        self.emit('; 26.3 Memory Database')
        self.emit('declare i64 @memdb_new(i64)')
        self.emit('declare i64 @memdb_store(i64, i64, i64, double)')
        self.emit('declare i64 @memdb_get(i64, i64)')
        self.emit('declare double @memdb_get_importance(i64, i64)')
        self.emit('declare i64 @memdb_set_importance(i64, i64, double)')
        self.emit('declare i64 @memdb_count(i64)')
        self.emit('declare void @memdb_close(i64)')
        self.emit('; 26.4 Clustering')
        self.emit('declare i64 @cluster_manager_new(double)')
        self.emit('declare i64 @cluster_add_memory(i64, i64, i64)')
        self.emit('declare i64 @cluster_get_for_memory(i64, i64)')
        self.emit('declare i64 @cluster_count(i64)')
        self.emit('declare i64 @cluster_member_count_cm(i64, i64)')
        self.emit('declare void @cluster_manager_close(i64)')
        self.emit('; 26.5 Pruning')
        self.emit('declare i64 @prune_config_new()')
        self.emit('declare i64 @prune_set_min_importance(i64, double)')
        self.emit('declare i64 @prune_set_max_age(i64, i64)')
        self.emit('declare i64 @prune_set_max_memories(i64, i64)')
        self.emit('declare i64 @prune_execute(i64, i64)')
        self.emit('declare void @prune_config_free(i64)')
        self.emit('; 26.6 Importance Scoring')
        self.emit('declare double @importance_calculate(double, double, double, double)')
        self.emit('declare double @importance_decay(double, double, double)')
        self.emit('declare double @importance_boost(double, double)')
        self.emit('; Phase 27: Belief System')
        self.emit('; 27.1 Belief Store')
        self.emit('declare i64 @belief_store_new()')
        self.emit('declare i64 @belief_store_add(i64, i64, double, i64)')
        self.emit('declare i64 @belief_store_get(i64, i64)')
        self.emit('declare double @belief_store_confidence(i64, i64)')
        self.emit('declare i64 @belief_store_remove(i64, i64)')
        self.emit('declare i64 @belief_store_count(i64)')
        self.emit('declare void @belief_store_close(i64)')
        self.emit('; 27.2 Contradiction Detection')
        self.emit('declare i64 @belief_check_contradiction(i64, i64, i64)')
        self.emit('declare i64 @belief_find_contradictions(i64)')
        self.emit('declare i64 @belief_contradiction_count(i64)')
        self.emit('; 27.3 Resolution Strategies')
        self.emit('declare i64 @belief_resolve_by_confidence(i64, i64, i64)')
        self.emit('declare i64 @belief_resolve_by_recency(i64, i64, i64)')
        self.emit('declare i64 @belief_resolve_by_source(i64, i64, i64)')
        self.emit('; 27.4 Semantic Queries')
        self.emit('declare i64 @belief_query_related(i64, i64)')
        self.emit('declare i64 @belief_query_by_source(i64, i64)')
        self.emit('declare i64 @belief_query_by_confidence(i64, double)')
        self.emit('; 27.5 Provenance')
        self.emit('declare i64 @belief_set_source(i64, i64, i64)')
        self.emit('declare i64 @belief_get_source(i64, i64)')
        self.emit('declare i64 @belief_set_timestamp(i64, i64, i64)')
        self.emit('declare i64 @belief_get_timestamp(i64, i64)')
        self.emit('; Phase 28: BDI Reasoning')
        self.emit('; 28.1 Goals')
        self.emit('declare i64 @goal_new(i64, i64)')
        self.emit('declare i64 @goal_set_priority(i64, double)')
        self.emit('declare double @goal_get_priority(i64)')
        self.emit('declare i64 @goal_set_deadline(i64, i64)')
        self.emit('declare i64 @goal_is_achieved(i64)')
        self.emit('declare void @goal_free(i64)')
        self.emit('; 28.2 Plans')
        self.emit('declare i64 @plan_new(i64)')
        self.emit('declare i64 @plan_add_step(i64, i64)')
        self.emit('declare i64 @plan_step_count(i64)')
        self.emit('declare i64 @plan_get_step(i64, i64)')
        self.emit('declare i64 @plan_set_precondition(i64, i64)')
        self.emit('declare i64 @plan_check_precondition(i64, i64)')
        self.emit('declare void @plan_free(i64)')
        self.emit('; 28.3 Intentions')
        self.emit('declare i64 @intention_new(i64, i64)')
        self.emit('declare i64 @intention_execute_step(i64)')
        self.emit('declare i64 @intention_is_complete(i64)')
        self.emit('declare i64 @intention_current_step(i64)')
        self.emit('declare i64 @intention_suspend(i64)')
        self.emit('declare i64 @intention_resume(i64)')
        self.emit('declare void @intention_free(i64)')
        self.emit('; 28.4 BDI Agent')
        self.emit('declare i64 @bdi_agent_new()')
        self.emit('declare i64 @bdi_add_belief(i64, i64, double)')
        self.emit('declare i64 @bdi_add_goal(i64, i64)')
        self.emit('declare i64 @bdi_add_plan(i64, i64, i64)')
        self.emit('declare i64 @bdi_deliberate(i64)')
        self.emit('declare i64 @bdi_execute(i64)')
        self.emit('declare i64 @bdi_goal_count(i64)')
        self.emit('declare i64 @bdi_intention_count(i64)')
        self.emit('declare void @bdi_agent_close(i64)')
        self.emit('; 28.5 Means-End Reasoning')
        self.emit('declare i64 @bdi_find_plans_for_goal(i64, i64)')
        self.emit('declare i64 @bdi_select_plan(i64, i64)')
        self.emit('declare i64 @bdi_commit_to_intention(i64, i64)')
        self.emit('; 28.6 Full BDI Reasoning (Phase 34.9.3)')
        self.emit('; Plan library with indexing')
        self.emit('declare i64 @plan_library_new()')
        self.emit('declare void @plan_library_add(i64, i64)')
        self.emit('declare i64 @plan_library_find(i64, i64)')           # find plans for goal
        self.emit('declare i64 @plan_library_index_rebuild(i64)')
        self.emit('; Commitment strategies (Bold/Cautious/Open)')
        self.emit('declare void @bdi_set_commitment(i64, i64)')         # set commitment strategy
        self.emit('declare i64 @commitment_bold()')                     # never drop
        self.emit('declare i64 @commitment_cautious()')                 # drop if impossible
        self.emit('declare i64 @commitment_open()')                     # drop if better option
        self.emit('; Reconsideration triggers')
        self.emit('declare void @bdi_set_reconsider(i64, i64)')         # set reconsider policy
        self.emit('declare i64 @bdi_should_reconsider(i64)')            # check if should reconsider
        self.emit('declare i64 @reconsider_never()')
        self.emit('declare i64 @reconsider_on_failure()')
        self.emit('declare i64 @reconsider_always()')
        self.emit('; Plan failure and replanning')
        self.emit('declare i64 @bdi_replan(i64, i64)')                  # replan for failed intention
        self.emit('declare void @bdi_mark_plan_failed(i64, i64)')
        self.emit('declare i64 @bdi_get_failed_count(i64, i64)')
        self.emit('; Full BDI interpreter loop')
        self.emit('declare i64 @bdi_run_cycle(i64)')                    # single BDI cycle
        self.emit('declare i64 @bdi_run_until_done(i64)')               # run until no intentions
        self.emit('declare void @bdi_set_max_cycles(i64, i64)')
        self.emit('; Phase 29: AI Integration')
        self.emit('; 29.1 LLM Client')
        self.emit('declare i64 @llm_client_new(i64)')
        self.emit('declare i64 @llm_set_api_key(i64, i64)')
        self.emit('declare i64 @llm_set_model(i64, i64)')
        self.emit('declare i64 @llm_complete(i64, i64)')
        self.emit('declare i64 @llm_chat(i64, i64)')
        self.emit('declare i64 @llm_embed(i64, i64)')
        self.emit('declare void @llm_client_close(i64)')
        self.emit('declare i64 @llm_set_base_url(i64, i64)')
        self.emit('; LLM Provider constants')
        self.emit('declare i64 @llm_provider_mock()')
        self.emit('declare i64 @llm_provider_anthropic()')
        self.emit('declare i64 @llm_provider_openai()')
        self.emit('declare i64 @llm_provider_ollama()')
        self.emit('; 29.2 Specialist Memory')
        self.emit('declare i64 @specialist_memory_new(i64)')
        self.emit('declare i64 @specialist_memory_store(i64, i64, i64)')
        self.emit('declare i64 @specialist_memory_recall(i64, i64, i64)')
        self.emit('declare i64 @specialist_memory_forget(i64, i64)')
        self.emit('declare i64 @specialist_memory_count(i64)')
        self.emit('declare void @specialist_memory_close(i64)')
        self.emit('; 29.3 Tool Registry')
        self.emit('declare i64 @tool_registry_new()')
        self.emit('declare i64 @tool_register(i64, i64, i64, i64)')
        self.emit('declare i64 @tool_get(i64, i64)')
        self.emit('declare i64 @tool_list(i64)')
        self.emit('declare i64 @tool_invoke(i64, i64, i64)')
        self.emit('declare void @tool_registry_close(i64)')
        self.emit('; Phase 30: Evolution')
        self.emit('; 30.1 Individuals')
        self.emit('declare i64 @individual_new(i64)')
        self.emit('declare i64 @individual_set_gene(i64, i64, double)')
        self.emit('declare double @individual_get_gene(i64, i64)')
        self.emit('declare i64 @individual_gene_count(i64)')
        self.emit('declare double @individual_fitness(i64)')
        self.emit('declare i64 @individual_set_fitness(i64, double)')
        self.emit('declare i64 @individual_clone(i64)')
        self.emit('declare void @individual_free(i64)')
        self.emit('; 30.2 Population')
        self.emit('declare i64 @population_new(i64)')
        self.emit('declare i64 @population_add(i64, i64)')
        self.emit('declare i64 @population_get(i64, i64)')
        self.emit('declare i64 @population_size(i64)')
        self.emit('declare i64 @population_best(i64)')
        self.emit('declare double @population_avg_fitness(i64)')
        self.emit('declare void @population_close(i64)')
        self.emit('; 30.3 Selection')
        self.emit('declare i64 @selection_tournament(i64, i64)')
        self.emit('declare i64 @selection_roulette(i64)')
        self.emit('declare i64 @selection_rank(i64)')
        self.emit('; 30.4 Crossover')
        self.emit('declare i64 @crossover_single_point(i64, i64)')
        self.emit('declare i64 @crossover_two_point(i64, i64)')
        self.emit('declare i64 @crossover_uniform(i64, i64, double)')
        self.emit('; 30.5 Mutation')
        self.emit('declare i64 @mutation_gaussian(i64, double)')
        self.emit('declare i64 @mutation_uniform(i64, double, double)')
        self.emit('declare i64 @mutation_bit_flip(i64, double)')
        self.emit('; 30.6 NSGA-II')
        self.emit('declare i64 @nsga2_new(i64, i64)')
        self.emit('declare i64 @nsga2_set_objective(i64, i64, i64)')
        self.emit('declare i64 @nsga2_evolve(i64, i64)')
        self.emit('declare i64 @nsga2_pareto_front(i64)')
        self.emit('declare void @nsga2_close(i64)')
        self.emit('; Phase 31: Distributed Intelligence')
        self.emit('; 31.1 Consensus')
        self.emit('declare i64 @consensus_new(i64)')
        self.emit('declare i64 @consensus_propose(i64, i64)')
        self.emit('declare i64 @consensus_accept(i64, i64)')
        self.emit('declare i64 @consensus_commit(i64)')
        self.emit('declare i64 @consensus_status(i64)')
        self.emit('declare void @consensus_close(i64)')
        self.emit('; 31.2 Stigmergy')
        self.emit('declare i64 @pheromone_new(i64, i64)')
        self.emit('declare i64 @pheromone_deposit(i64, i64, i64, double)')
        self.emit('declare double @pheromone_read(i64, i64, i64)')
        self.emit('declare i64 @pheromone_evaporate(i64, double)')
        self.emit('declare void @pheromone_close(i64)')
        self.emit('; 31.3 Swarm')
        self.emit('declare i64 @swarm_new(i64, i64)')
        self.emit('declare i64 @swarm_set_position(i64, i64, i64, double)')
        self.emit('declare double @swarm_get_position(i64, i64, i64)')
        self.emit('declare i64 @swarm_set_velocity(i64, i64, i64, double)')
        self.emit('declare i64 @swarm_update(i64)')
        self.emit('declare i64 @swarm_best_particle(i64)')
        self.emit('declare double @swarm_best_fitness(i64)')
        self.emit('declare void @swarm_close(i64)')
        self.emit('; 31.4 Voting')
        self.emit('declare i64 @voting_new(i64)')
        self.emit('declare i64 @voting_add_option(i64, i64)')
        self.emit('declare i64 @voting_cast(i64, i64, i64)')
        self.emit('declare i64 @voting_tally(i64)')
        self.emit('declare i64 @voting_winner(i64)')
        self.emit('declare i64 @voting_result(i64, i64)')
        self.emit('declare void @voting_close(i64)')
        self.emit('; Phase 24.5: f64 Math Intrinsics')
        self.emit('declare double @f64_add(double, double)')
        self.emit('declare double @f64_sub(double, double)')
        self.emit('declare double @f64_mul(double, double)')
        self.emit('declare double @f64_div(double, double)')
        self.emit('declare double @f64_neg(double)')
        self.emit('declare double @f64_abs(double)')
        self.emit('declare i64 @f64_eq(double, double)')
        self.emit('declare i64 @f64_ne(double, double)')
        self.emit('declare i64 @f64_lt(double, double)')
        self.emit('declare i64 @f64_le(double, double)')
        self.emit('declare i64 @f64_gt(double, double)')
        self.emit('declare i64 @f64_ge(double, double)')
        self.emit('declare double @f64_sqrt(double)')
        self.emit('declare double @f64_pow(double, double)')
        self.emit('declare double @f64_sin(double)')
        self.emit('declare double @f64_cos(double)')
        self.emit('declare double @f64_tan(double)')
        self.emit('declare double @f64_asin(double)')
        self.emit('declare double @f64_acos(double)')
        self.emit('declare double @f64_atan(double)')
        self.emit('declare double @f64_atan2(double, double)')
        self.emit('declare double @f64_exp(double)')
        self.emit('declare double @f64_log(double)')
        self.emit('declare double @f64_log10(double)')
        self.emit('declare double @f64_log2(double)')
        self.emit('declare double @f64_floor(double)')
        self.emit('declare double @f64_ceil(double)')
        self.emit('declare double @f64_round(double)')
        self.emit('declare double @f64_trunc(double)')
        self.emit('declare double @f64_min(double, double)')
        self.emit('declare double @f64_max(double, double)')
        self.emit('declare double @f64_from_i64(i64)')
        self.emit('declare i64 @f64_to_i64(double)')
        self.emit('declare ptr @f64_to_string(double)')
        self.emit('declare double @f64_from_string(ptr)')

        self.emit('; Phase 34 Wave 6: Low Priority Items')
        self.emit('; 34.2.4 Vector<T, N> Fixed-Size Type (SIMD)')
        self.emit('declare i64 @simd_vec_new(i64)')                    # new(size)
        self.emit('declare void @simd_vec_set(i64, i64, double)')      # set(vec, idx, val)
        self.emit('declare double @simd_vec_get(i64, i64)')            # get(vec, idx)
        self.emit('declare double @simd_dot_product(i64, i64)')        # dot product
        self.emit('declare i64 @simd_normalize(i64)')                  # normalize in place
        self.emit('declare i64 @simd_add(i64, i64)')                   # vector add
        self.emit('declare i64 @simd_sub(i64, i64)')                   # vector subtract
        self.emit('declare i64 @simd_mul_scalar(i64, double)')         # scalar multiply
        self.emit('declare double @simd_magnitude(i64)')               # vector magnitude

        self.emit('; 34.2.5 Tensor<T> Type')
        self.emit('declare i64 @tensor_new(i64, i64)')                 # new(shape, dtype)
        self.emit('declare void @tensor_set(i64, i64, double)')        # set(tensor, indices, val)
        self.emit('declare double @tensor_get(i64, i64)')              # get(tensor, indices)
        self.emit('declare i64 @tensor_reshape(i64, i64)')             # reshape(tensor, new_shape)
        self.emit('declare i64 @tensor_broadcast(i64, i64)')           # broadcast(tensor, shape)
        self.emit('declare i64 @tensor_matmul(i64, i64)')              # matrix multiply
        self.emit('declare i64 @tensor_transpose(i64)')                # transpose
        self.emit('declare i64 @tensor_sum(i64, i64)')                 # sum along axis

        self.emit('; 34.4.4 Actor Metrics')
        self.emit('declare i64 @actor_metrics_new(i64)')               # new(actor_id)
        self.emit('declare i64 @actor_metrics_message_count(i64)')     # total messages
        self.emit('declare i64 @actor_metrics_throughput(i64)')        # messages/sec
        self.emit('declare double @actor_metrics_latency_avg(i64)')    # avg latency
        self.emit('declare double @actor_metrics_latency_p99(i64)')    # p99 latency
        self.emit('declare i64 @actor_metrics_export_prometheus(i64)') # export to prometheus

        self.emit('; 34.8.5 Debugger (DAP)')
        self.emit('declare i64 @dap_server_new()')                     # create DAP server
        self.emit('declare void @dap_server_start(i64, i64)')          # start(server, port)
        self.emit('declare i64 @dap_set_breakpoint(i64, i64, i64)')    # set_bp(file, line)
        self.emit('declare void @dap_clear_breakpoint(i64)')           # clear breakpoint
        self.emit('declare i64 @dap_step_over(i64)')                   # step over
        self.emit('declare i64 @dap_step_into(i64)')                   # step into
        self.emit('declare i64 @dap_step_out(i64)')                    # step out
        self.emit('declare i64 @dap_get_variables(i64, i64)')          # get variables in scope
        self.emit('declare i64 @dap_get_callstack(i64)')               # get call stack
        self.emit('declare i64 @dap_evaluate(i64, i64)')               # evaluate expression

        self.emit('; 34.9.5 Evolution/Fine-tuning')
        self.emit('declare i64 @lora_adapter_new(i64)')                # create LoRA adapter
        self.emit('declare i64 @lora_train(i64, i64, i64)')            # train adapter
        self.emit('declare i64 @lora_apply(i64, i64)')                 # apply to model
        self.emit('declare i64 @knowledge_distill(i64, i64, i64)')     # distill knowledge
        self.emit('declare i64 @model_prune(i64, double)')             # prune weights

        self.emit('; 34.10.1 DNS Improvements')
        self.emit('declare i64 @dns_resolve_async(i64)')               # async resolve
        self.emit('declare i64 @dns_cache_get(i64)')                   # get from cache
        self.emit('declare void @dns_cache_set(i64, i64, i64)')        # set with TTL
        self.emit('declare i64 @dns_query_type(i64, i64)')             # query(name, type)
        self.emit('declare i64 @dns_over_https(i64)')                  # DNS-over-HTTPS
        self.emit('declare void @dns_set_nameserver(i64)')             # custom nameserver

        self.emit('; 34.10.2 AGM Belief Revision')
        self.emit('declare i64 @agm_closure(i64)')                     # logical closure
        self.emit('declare i64 @agm_contract(i64, i64)')               # contract belief
        self.emit('declare i64 @agm_revise(i64, i64)')                 # revise with new belief
        self.emit('declare i64 @agm_expand(i64, i64)')                 # expand belief set
        self.emit('declare i64 @agm_entrenchment(i64, i64)')           # get entrenchment order

        self.emit('; 34.10.5 Neuroevolution')
        self.emit('declare i64 @neat_genome_new()')                    # create NEAT genome
        self.emit('declare i64 @neat_add_node(i64, i64)')              # add node mutation
        self.emit('declare i64 @neat_add_connection(i64, i64, i64, double)') # add connection
        self.emit('declare i64 @neat_mutate_weights(i64, double)')     # mutate weights
        self.emit('declare double @neat_compatibility(i64, i64)')      # compatibility distance
        self.emit('declare i64 @neat_crossover(i64, i64)')             # crossover genomes
        self.emit('declare i64 @neat_speciate(i64, double)')           # speciate population

        self.emit('; 34.10.7 Full Swarm Algorithms')
        self.emit('declare i64 @aco_new(i64, i64)')                    # Ant Colony Optimization
        self.emit('declare void @aco_deposit_pheromone(i64, i64, double)') # deposit pheromone
        self.emit('declare double @aco_get_pheromone(i64, i64)')       # get pheromone level
        self.emit('declare void @aco_evaporate(i64, double)')          # evaporate pheromones
        self.emit('declare i64 @aco_best_path(i64)')                   # get best path
        self.emit('declare i64 @bee_algorithm_new(i64)')               # Bee algorithm
        self.emit('declare i64 @firefly_algorithm_new(i64)')           # Firefly algorithm

        self.emit('; 34.11.9 HTTP Enhancements')
        self.emit('declare i64 @http_cookie_jar_new()')                # cookie jar
        self.emit('declare void @http_cookie_set(i64, i64, i64)')      # set cookie
        self.emit('declare i64 @http_cookie_get(i64, i64)')            # get cookie
        self.emit('declare i64 @http_compress_gzip(i64)')              # gzip compress
        self.emit('declare i64 @http_decompress_gzip(i64)')            # gzip decompress
        self.emit('declare void @http_retry_config(i64, i64, i64)')    # retry with backoff

        # NOTE: Parser Error Recovery and REPL functions are user-defined in parser.sx/repl.sx,
        # not runtime intrinsics. Do not declare them here to avoid conflicts.

        self.emit('; Phase 11: Debugger/DAP')
        self.emit('declare i64 @debugger_new()')
        self.emit('declare i64 @debugger_set_breakpoint(i64, i64, i64)')
        self.emit('declare i64 @debugger_remove_breakpoint(i64, i64)')
        self.emit('declare i64 @debugger_enable_breakpoint(i64, i64, i64)')
        self.emit('declare i64 @debugger_set_condition(i64, i64, i64)')
        self.emit('declare i64 @debugger_breakpoint_count(i64)')
        self.emit('declare i64 @debugger_at_breakpoint(i64, i64, i64)')
        self.emit('declare i64 @debugger_push_frame(i64, i64, i64, i64)')
        self.emit('declare i64 @debugger_pop_frame(i64)')
        self.emit('declare i64 @debugger_frame_count(i64)')
        self.emit('declare i64 @debugger_frame_name(i64, i64)')
        self.emit('declare i64 @debugger_add_variable(i64, i64, i64, i64)')
        self.emit('declare i64 @debugger_variable_count(i64, i64)')
        self.emit('declare i64 @debugger_variable_name(i64, i64, i64)')
        self.emit('declare i64 @debugger_variable_value(i64, i64, i64)')
        self.emit('declare i64 @debugger_pause(i64)')
        self.emit('declare i64 @debugger_continue(i64)')
        self.emit('declare i64 @debugger_step_over(i64)')
        self.emit('declare i64 @debugger_step_into(i64)')
        self.emit('declare i64 @debugger_step_out(i64)')
        self.emit('declare i64 @debugger_is_paused(i64)')
        self.emit('declare i64 @debugger_set_location(i64, i64, i64)')
        self.emit('declare i64 @debugger_current_line(i64)')
        self.emit('declare void @debugger_close(i64)')
        self.emit('; Phase 11: Cursus VM')
        self.emit('declare i64 @vm_new(i64, i64)')
        self.emit('declare i64 @vm_load(i64, i64, i64)')
        self.emit('declare i64 @vm_step(i64)')
        self.emit('declare i64 @vm_run(i64)')
        self.emit('declare i64 @vm_sp(i64)')
        self.emit('declare i64 @vm_top(i64)')
        self.emit('declare i64 @vm_error(i64)')
        self.emit('declare i64 @vm_ip(i64)')
        self.emit('declare i64 @vm_set_local(i64, i64, i64)')
        self.emit('declare i64 @vm_get_local(i64, i64)')
        self.emit('declare i64 @vm_reset(i64)')
        self.emit('declare void @vm_close(i64)')
        self.emit('declare i64 @vm_op_nop()')
        self.emit('declare i64 @vm_op_push()')
        self.emit('declare i64 @vm_op_pop()')
        self.emit('declare i64 @vm_op_dup()')
        self.emit('declare i64 @vm_op_add()')
        self.emit('declare i64 @vm_op_sub()')
        self.emit('declare i64 @vm_op_mul()')
        self.emit('declare i64 @vm_op_div()')
        self.emit('declare i64 @vm_op_mod()')
        self.emit('declare i64 @vm_op_neg()')
        self.emit('declare i64 @vm_op_eq()')
        self.emit('declare i64 @vm_op_ne()')
        self.emit('declare i64 @vm_op_lt()')
        self.emit('declare i64 @vm_op_le()')
        self.emit('declare i64 @vm_op_gt()')
        self.emit('declare i64 @vm_op_ge()')
        self.emit('declare i64 @vm_op_jmp()')
        self.emit('declare i64 @vm_op_jz()')
        self.emit('declare i64 @vm_op_jnz()')
        self.emit('declare i64 @vm_op_load()')
        self.emit('declare i64 @vm_op_store()')
        self.emit('declare i64 @vm_op_call()')
        self.emit('declare i64 @vm_op_ret()')
        self.emit('declare i64 @vm_op_print()')
        self.emit('declare i64 @vm_op_halt()')
        self.emit('; Phase 11: Cross-compilation')
        self.emit('declare i64 @target_parse(i64)')
        self.emit('declare i64 @target_arch(i64)')
        self.emit('declare i64 @target_os(i64)')
        self.emit('declare i64 @target_env(i64)')
        self.emit('declare i64 @target_pointer_size(i64)')
        self.emit('declare i64 @target_is_little_endian(i64)')
        self.emit('declare i64 @target_triple_string(i64)')
        self.emit('declare i64 @target_llvm_triple(i64)')
        self.emit('declare i64 @target_data_layout(i64)')
        self.emit('declare void @target_close(i64)')
        self.emit('declare i64 @target_host()')
        self.emit('declare i64 @arch_x86_64()')
        self.emit('declare i64 @arch_aarch64()')
        self.emit('declare i64 @arch_riscv64()')
        self.emit('declare i64 @arch_wasm32()')
        self.emit('declare i64 @os_linux()')
        self.emit('declare i64 @os_macos()')
        self.emit('declare i64 @os_windows()')
        self.emit('declare i64 @os_freebsd()')
        self.emit('declare i64 @os_wasi()')
        self.emit('declare i64 @env_gnu()')
        self.emit('declare i64 @env_musl()')
        self.emit('declare i64 @env_msvc()')
        self.emit('declare i64 @env_none()')
        self.emit('')

        # First pass: handle module declarations and imports
        for item in items:
            if item['type'] == 'ModDef':
                # Load module from file
                mod_name = item['name']
                self.load_module(mod_name)
            elif item['type'] == 'UseDef':
                # Import items from module
                path = item['path']
                self.import_from_path(path)

        # Second pass: register enums, structs, actors, and specialists
        for item in items:
            if item['type'] == 'EnumDef':
                variants = item['variants']
                if variants and isinstance(variants[0], dict):
                    self.enums[item['name']] = {v['name']: i for i, v in enumerate(variants)}
                else:
                    self.enums[item['name']] = {v: i for i, v in enumerate(variants)}
            elif item['type'] == 'StructDef':
                # Check if it's a generic struct
                if item.get('type_params'):
                    if not hasattr(self, 'generic_structs'):
                        self.generic_structs = {}
                    self.generic_structs[item['name']] = item
                else:
                    self.structs[item['name']] = item['fields']
            elif item['type'] == 'ActorDef':
                # Register actor as a struct with its state variables
                self.actors[item['name']] = item
                self.structs[item['name']] = [(v['name'], v['ty']) for v in item['state_vars']]
            elif item['type'] == 'SpecialistDef':
                # Register specialist like actor (specialist is actor + AI)
                self.specialists[item['name']] = item
                self.actors[item['name']] = item  # Specialists are actors
                self.structs[item['name']] = [(v['name'], v['ty']) for v in item['state_vars']]
            elif item['type'] == 'HiveDef':
                self.hives[item['name']] = item
            elif item['type'] == 'TraitDef':
                self.traits[item['name']] = item
            elif item['type'] == 'ImplDef' and item.get('trait_name'):
                # Register trait implementation for type
                self.trait_impls[(item['trait_name'], item['type_name'])] = item

        # Generate vtables for trait implementations (dyn Trait support)
        self.vtables = {}  # (trait_name, type_name) -> vtable_global_name
        for (trait_name, type_name), impl_def in self.trait_impls.items():
            self.generate_vtable(trait_name, type_name, impl_def)

        # Second pass: generate code
        for item in items:
            if item['type'] == 'FnDef':
                # Register generic functions but don't generate them yet
                if item.get('type_params'):
                    self.generic_fns[item['name']] = item
                else:
                    self.generate_fn(item)
            elif item['type'] == 'GenFnDef':
                # Generate generator function (state machine)
                self.generate_gen_fn(item)
            elif item['type'] == 'ImplDef':
                self.generate_impl(item)
            elif item['type'] == 'ActorDef':
                self.generate_actor(item)
            elif item['type'] == 'SpecialistDef':
                self.generate_specialist(item)
            elif item['type'] == 'HiveDef':
                self.generate_hive(item)

        # Generate pending generic instantiations
        while self.pending_instantiations:
            mangled_name, fn_def, type_args = self.pending_instantiations.pop(0)
            self.generate_fn_instantiation(fn_def, mangled_name, type_args)

        # Generate code for imported modules
        if hasattr(self, 'module_items'):
            for mod_name, mod_items in self.module_items.items():
                self.emit('')
                self.emit(f'; Module: {mod_name}')
                for item in mod_items:
                    if item['type'] == 'FnDef':
                        if not item.get('type_params'):  # Skip generic functions
                            self.generate_fn(item)
                    elif item['type'] == 'ImplDef':
                        self.generate_impl(item)

        # Add closure functions
        if self.pending_closures:
            self.emit('')
            self.emit('; Closure functions')
            for closure_code in self.pending_closures:
                self.emit(closure_code)

        # Add string constants at the end
        if self.string_constants:
            self.emit('')
            self.emit('; String constants')
            for label, value in self.string_constants:
                # Escape string for LLVM
                escaped = ''
                for c in value:
                    if c == '\n':
                        escaped += '\\0A'
                    elif c == '\t':
                        escaped += '\\09'
                    elif c == '"':
                        escaped += '\\22'
                    elif c == '\\':
                        escaped += '\\5C'
                    elif ord(c) < 32 or ord(c) > 126:
                        escaped += f'\\{ord(c):02X}'
                    else:
                        escaped += c
                escaped += '\\00'  # null terminator
                self.emit(f'{label} = private unnamed_addr constant [{len(value)+1} x i8] c"{escaped}"')

        return '\n'.join(self.output)

    def new_local(self, name):
        local = f"%local.{name}.{self.local_counter}"
        self.local_counter += 1
        return local

    def collect_locals(self, node):
        """Pre-pass to collect all local variable names from the AST in order."""
        locals_list = []
        if isinstance(node, dict):
            if node.get('type') == 'LetStmt':
                locals_list.append(node['name'])
            # Traverse in consistent order: stmts first, then expr, then other fields
            if 'stmts' in node:
                for stmt in node['stmts']:
                    locals_list.extend(self.collect_locals(stmt))
            if 'expr' in node and node['expr']:
                locals_list.extend(self.collect_locals(node['expr']))
            for key, value in node.items():
                if key not in ('stmts', 'expr', 'type', 'name', 'value'):
                    locals_list.extend(self.collect_locals(value))
        elif isinstance(node, list):
            for item in node:
                locals_list.extend(self.collect_locals(item))
        return locals_list

    def mangle_generic_name(self, name, type_args):
        """Mangle a generic function name with its type arguments.
        Handles nested types like Option<Vec<i64>> by replacing special chars."""
        def mangle_type(ty):
            if isinstance(ty, str):
                # Replace special characters for LLVM identifier compatibility
                return ty.replace('<', '$').replace('>', '$').replace(',', '_').replace(' ', '')
            return str(ty)
        mangled_args = '_'.join(mangle_type(arg) for arg in type_args)
        return f"{name}_${mangled_args}$"

    def mangle_struct_name(self, name, type_args):
        """Mangle a generic struct name with its type arguments."""
        def mangle_type(ty):
            if isinstance(ty, str):
                return ty.replace('<', '$').replace('>', '$').replace(',', '_').replace(' ', '')
            return str(ty)
        mangled_args = '_'.join(mangle_type(arg) for arg in type_args)
        return f"{name}_${mangled_args}$"

    def substitute_type(self, ty):
        """Substitute type parameters with concrete types using current type_subst map."""
        if not hasattr(self, 'type_subst') or not self.type_subst:
            return ty
        if ty in self.type_subst:
            return self.type_subst[ty]
        # Handle generic types like Vec<T>
        if '<' in ty and '>' in ty:
            base = ty[:ty.index('<')]
            args_str = ty[ty.index('<')+1:ty.rindex('>')]
            # Parse type arguments (handle nested generics)
            args = self.parse_type_args(args_str)
            substituted_args = [self.substitute_type(arg) for arg in args]
            return f"{base}<{', '.join(substituted_args)}>"
        return ty

    def parse_type_args(self, args_str):
        """Parse comma-separated type arguments, handling nested generics."""
        args = []
        depth = 0
        current = ''
        for char in args_str:
            if char == '<':
                depth += 1
                current += char
            elif char == '>':
                depth -= 1
                current += char
            elif char == ',' and depth == 0:
                args.append(current.strip())
                current = ''
            else:
                current += char
        if current.strip():
            args.append(current.strip())
        return args

    def queue_generic_instantiation(self, name, type_args):
        """Queue a generic function for instantiation and return mangled name."""
        if name not in self.generic_fns:
            return name  # Not a generic function

        # Normalize type arguments (apply current substitutions)
        resolved_args = tuple(self.substitute_type(arg) if hasattr(self, 'type_subst') else arg for arg in type_args)

        mangled_name = self.mangle_generic_name(name, resolved_args)
        if mangled_name not in self.instantiated:
            self.instantiated.add(mangled_name)
            fn_def = self.generic_fns[name]
            self.pending_instantiations.append((mangled_name, fn_def, resolved_args))
        return mangled_name

    def queue_struct_instantiation(self, name, type_args):
        """Queue a generic struct for instantiation and return mangled name."""
        if not hasattr(self, 'generic_structs'):
            self.generic_structs = {}
        if not hasattr(self, 'instantiated_structs'):
            self.instantiated_structs = set()

        if name not in self.generic_structs:
            return name  # Not a generic struct

        # Normalize type arguments
        resolved_args = tuple(self.substitute_type(arg) if hasattr(self, 'type_subst') else arg for arg in type_args)

        mangled_name = self.mangle_struct_name(name, resolved_args)
        if mangled_name not in self.instantiated_structs:
            self.instantiated_structs.add(mangled_name)
            struct_def = self.generic_structs[name]
            # Create instantiated struct with substituted field types
            type_params = struct_def.get('type_params', [])
            subst = dict(zip(type_params, resolved_args))
            instantiated_fields = []
            for field_name, field_type in struct_def.get('fields', []):
                subst_type = subst.get(field_type, field_type)
                instantiated_fields.append((field_name, subst_type))
            self.structs[mangled_name] = instantiated_fields
        return mangled_name

    def generate_fn_instantiation(self, fn, mangled_name, type_args):
        """Generate a monomorphized instantiation of a generic function."""
        self.locals = {}
        self.var_types = {}
        self.temp_counter = 0
        self.label_counter = 0
        self.local_counter = 0
        self.pre_alloca_queue = []
        self.const_params = {}  # Clear for each instantiation

        # Create type substitution map
        type_params = fn.get('type_params', [])
        self.type_subst = dict(zip(type_params, type_args))
        self.current_type_params = set(type_params)

        # Set up const params - match type_params with "const:" prefix to type_args
        for i, type_param in enumerate(type_params):
            if type_param.startswith('const:'):
                const_name = type_param[6:]  # Remove "const:" prefix
                if i < len(type_args):
                    self.const_params[const_name] = type_args[i]

        ret_type = self.type_to_llvm(fn['return_type'])
        self.current_fn_return_type = ret_type

        # Build param list
        params = fn['params']
        params_str = ', '.join(f"{self.type_to_llvm(p['ty'])} %param.{p['name']}" for p in params)

        self.emit(f'define {ret_type} @"{mangled_name}"({params_str}) {{')
        self.emit('entry:')

        # Pre-allocate local variables
        all_locals = self.collect_locals(fn['body'])
        for local_name in all_locals:
            local = self.new_local(local_name)
            self.emit(f'  {local} = alloca i64')
            self.pre_alloca_queue.append(local)

        # Allocate space for params and copy them
        for p in params:
            local = self.new_local(p['name'])
            self.emit(f'  {local} = alloca i64')
            self.emit(f'  store i64 %param.{p["name"]}, ptr {local}')
            self.locals[p['name']] = local
            # Record type with substitution for generic params
            param_type = p.get('ty', 'i64')
            if hasattr(self, 'type_subst') and param_type in self.type_subst:
                param_type = self.type_subst[param_type]
            self.var_types[p['name']] = param_type

        result = self.generate_block(fn['body'])

        if ret_type == 'void':
            self.emit('  ret void')
        else:
            self.emit(f'  ret {ret_type} {result}')

        self.emit('}')
        self.emit('')

        # Clean up
        self.type_subst = {}
        self.const_params = {}

    def find_await_points(self, node, await_points=None):
        """Find all await expressions in an AST node and return list of (path, node) tuples."""
        if await_points is None:
            await_points = []

        if isinstance(node, dict):
            if node.get('type') == 'AwaitExpr':
                await_points.append(node)
            for key, value in node.items():
                if key != 'type':
                    self.find_await_points(value, await_points)
        elif isinstance(node, list):
            for item in node:
                self.find_await_points(item, await_points)

        return await_points

    def find_free_variables(self, node, bound_vars=None, free_vars=None):
        """Find all free variables (identifiers not bound by local let or params) in an expression."""
        if bound_vars is None:
            bound_vars = set()
        if free_vars is None:
            free_vars = []

        if isinstance(node, dict):
            node_type = node.get('type')

            if node_type == 'IdentExpr':
                name = node.get('name')
                if name and name not in bound_vars and name not in free_vars:
                    # Skip special names
                    if name not in ('self', 'true', 'false'):
                        free_vars.append(name)

            elif node_type == 'LetStmt':
                # Process value first (before binding), then add binding
                if node.get('value'):
                    self.find_free_variables(node['value'], bound_vars, free_vars)
                new_bound = bound_vars | {node['name']}
                return free_vars  # Don't recurse further

            elif node_type == 'Block':
                # Process statements with accumulated bindings
                new_bound = set(bound_vars)
                for stmt in node.get('stmts', []):
                    if stmt.get('type') == 'LetStmt':
                        if stmt.get('value'):
                            self.find_free_variables(stmt['value'], new_bound, free_vars)
                        new_bound.add(stmt['name'])
                    else:
                        self.find_free_variables(stmt, new_bound, free_vars)
                if node.get('expr'):
                    self.find_free_variables(node['expr'], new_bound, free_vars)
                return free_vars

            elif node_type == 'ForExpr':
                # var_name is bound within the body
                self.find_free_variables(node.get('start'), bound_vars, free_vars)
                self.find_free_variables(node.get('end'), bound_vars, free_vars)
                new_bound = bound_vars | {node.get('var_name', '')}
                self.find_free_variables(node.get('body'), new_bound, free_vars)
                return free_vars

            elif node_type in ('ClosureExpr', 'AsyncClosureExpr'):
                # Closure params are bound within closure body, but we MUST recurse
                # to find free variables that nested closures need (transitive capture)
                closure_params = {p['name'] for p in node.get('params', [])}
                new_bound = bound_vars | closure_params
                self.find_free_variables(node.get('body'), new_bound, free_vars)
                return free_vars

            elif node_type == 'CallExpr':
                # Skip function name, process args
                for arg in node.get('args', []):
                    self.find_free_variables(arg, bound_vars, free_vars)
                return free_vars

            else:
                # Recurse into all values
                for key, value in node.items():
                    if key not in ('type', 'name') or key == 'name' and node_type not in ('IdentExpr', 'LetStmt', 'ForExpr'):
                        self.find_free_variables(value, bound_vars, free_vars)

        elif isinstance(node, list):
            for item in node:
                self.find_free_variables(item, bound_vars, free_vars)

        return free_vars

    def collect_live_vars_across_await(self, body, await_count):
        """Collect variables that need to be saved across await points."""
        # For simplicity, save all locals - a more sophisticated analysis
        # would track which variables are live across each await
        return self.collect_locals(body)

    def generate_async_fn(self, fn):
        """Generate async function as a future constructor + poll function with proper state machine."""
        name = fn['name']
        params = fn['params']
        return_type = fn['return_type']

        self.locals = {}
        self.var_types = {}
        self.temp_counter = 0
        self.label_counter = 0
        self.local_counter = 0
        self.pre_alloca_queue = []
        self.is_in_async_fn = True

        # Find all await points in the function body
        await_points = self.find_await_points(fn['body'])
        num_states = len(await_points) + 1  # states 0..n where n is number of awaits

        # Collect all locals that need to be saved across await boundaries
        all_locals = self.collect_locals(fn['body'])
        num_params = len(params)
        num_locals = len(all_locals)

        # Future struct layout:
        # offset 0:  poll_fn pointer (i64)
        # offset 8:  state number (i64)
        # offset 16: inner_future (i64) - for storing awaited future
        # offset 24: result (i64)
        # offset 32+: params (8 bytes each)
        # offset 32+num_params*8: locals (8 bytes each)
        base_size = 32
        params_offset = base_size
        locals_offset = params_offset + num_params * 8
        struct_size = locals_offset + num_locals * 8

        # Generate the constructor function that creates the future
        params_str = ', '.join(f"i64 %param.{p['name']}" for p in params)
        self.emit(f'; Async function {name} - {num_states} states, {len(await_points)} await points')
        self.emit(f'define i64 @"{name}"({params_str}) {{')
        self.emit('entry:')

        self.emit(f'  %future = call ptr @malloc(i64 {struct_size})')

        # Store poll function pointer at offset 0
        self.emit(f'  %poll_fn_ptr = ptrtoint ptr @"{name}_poll" to i64')
        self.emit(f'  store i64 %poll_fn_ptr, ptr %future')

        # Store initial state (0) at offset 8
        self.emit(f'  %state_ptr = getelementptr i8, ptr %future, i64 8')
        self.emit(f'  store i64 0, ptr %state_ptr')

        # Initialize inner_future to 0 at offset 16
        self.emit(f'  %inner_ptr = getelementptr i8, ptr %future, i64 16')
        self.emit(f'  store i64 0, ptr %inner_ptr')

        # Store parameters in future struct
        for i, p in enumerate(params):
            offset = params_offset + i * 8
            self.emit(f'  %param_ptr_{i} = getelementptr i8, ptr %future, i64 {offset}')
            self.emit(f'  store i64 %param.{p["name"]}, ptr %param_ptr_{i}')

        # Initialize locals to 0
        for i in range(num_locals):
            offset = locals_offset + i * 8
            self.emit(f'  %local_init_ptr_{i} = getelementptr i8, ptr %future, i64 {offset}')
            self.emit(f'  store i64 0, ptr %local_init_ptr_{i}')

        self.emit(f'  %result = ptrtoint ptr %future to i64')
        self.emit(f'  ret i64 %result')
        self.emit('}')
        self.emit('')

        # Generate the poll function with state machine
        self.locals = {}
        self.temp_counter = 0
        self.label_counter = 0
        self.local_counter = 0
        self.pre_alloca_queue = []
        self.async_state_counter = 0
        self.async_locals_offset = locals_offset
        self.async_all_locals = all_locals
        self.async_future_ptr = '%future'

        self.emit(f'define i64 @"{name}_poll"(i64 %future_ptr) {{')
        self.emit('entry:')
        self.emit('  %future = inttoptr i64 %future_ptr to ptr')

        # Load current state from offset 8
        self.emit('  %state_ptr = getelementptr i8, ptr %future, i64 8')
        self.emit('  %state = load i64, ptr %state_ptr')

        # Allocate stack space for parameters and locals
        for i, p in enumerate(params):
            offset = params_offset + i * 8
            local = self.new_local(p['name'])
            self.emit(f'  {local} = alloca i64')
            self.emit(f'  %param_load_ptr_{i} = getelementptr i8, ptr %future, i64 {offset}')
            self.emit(f'  %param_val_{i} = load i64, ptr %param_load_ptr_{i}')
            self.emit(f'  store i64 %param_val_{i}, ptr {local}')
            self.locals[p['name']] = local

        # Pre-allocate locals and load their saved values from future struct
        for i, local_name in enumerate(all_locals):
            local = self.new_local(local_name)
            self.emit(f'  {local} = alloca i64')
            offset = locals_offset + i * 8
            self.emit(f'  %local_load_ptr_{i} = getelementptr i8, ptr %future, i64 {offset}')
            self.emit(f'  %local_val_{i} = load i64, ptr %local_load_ptr_{i}')
            self.emit(f'  store i64 %local_val_{i}, ptr {local}')
            self.pre_alloca_queue.append(local)

        # Generate state machine switch
        state_labels = [f'state_{i}' for i in range(num_states)]
        default_label = 'state_invalid'

        if num_states > 1:
            self.emit(f'  switch i64 %state, label %{default_label} [')
            for i in range(num_states):
                self.emit(f'    i64 {i}, label %{state_labels[i]}')
            self.emit('  ]')
            self.emit(f'{default_label}:')
            self.emit('  ret i64 0  ; Invalid state returns Pending')
        else:
            self.emit(f'  br label %{state_labels[0]}')

        # Generate state 0 - initial execution
        self.emit(f'{state_labels[0]}:')

        # Track which await we're at during code generation
        self.current_await_index = 0
        self.state_labels = state_labels
        self.num_states = num_states
        self.async_locals_offset = locals_offset
        self.async_all_locals = all_locals

        result = self.generate_block(fn['body'])

        # Save locals back to future struct before returning
        for i, local_name in enumerate(all_locals):
            if local_name in self.locals:
                offset = locals_offset + i * 8
                local = self.locals[local_name]
                save_val = self.new_temp()
                self.emit(f'  {save_val} = load i64, ptr {local}')
                save_ptr = self.new_temp()
                self.emit(f'  {save_ptr} = getelementptr i8, ptr %future, i64 {offset}')
                self.emit(f'  store i64 {save_val}, ptr {save_ptr}')

        # Return Ready with value (tagged: value << 1 | 1)
        ret_t = self.new_temp()
        self.emit(f'  {ret_t} = shl i64 {result}, 1')
        tagged_t = self.new_temp()
        self.emit(f'  {tagged_t} = or i64 {ret_t}, 1')
        self.emit(f'  ret i64 {tagged_t}')

        self.emit('}')
        self.emit('')

        self.is_in_async_fn = False
        # Clean up async-specific state
        if hasattr(self, 'async_state_counter'):
            del self.async_state_counter
        if hasattr(self, 'state_labels'):
            del self.state_labels

    def find_yield_points(self, node, yield_points=None):
        """Find all yield expressions in an AST node for generator state machine."""
        if yield_points is None:
            yield_points = []
        if isinstance(node, dict):
            if node.get('type') == 'YieldExpr':
                yield_points.append(node)
            for key, value in node.items():
                self.find_yield_points(value, yield_points)
        elif isinstance(node, list):
            for item in node:
                self.find_yield_points(item, yield_points)
        return yield_points

    def generate_gen_fn(self, fn):
        """Generate generator function as a Stream state machine.
        Similar to async fn but yields values instead of awaiting."""
        name = fn['name']
        params = fn['params']
        yield_type = fn.get('yield_type', 'i64')

        self.locals = {}
        self.var_types = {}
        self.temp_counter = 0
        self.label_counter = 0
        self.local_counter = 0
        self.pre_alloca_queue = []
        self.is_in_gen_fn = True

        # Find all yield points in the function body
        yield_points = self.find_yield_points(fn['body'])
        num_states = len(yield_points) + 1  # states 0..n where n is number of yields

        # Collect all locals
        all_locals = self.collect_locals(fn['body'])
        num_params = len(params)
        num_locals = len(all_locals)

        # Generator struct layout:
        # offset 0:  next_fn pointer (i64) - for calling to get next value
        # offset 8:  state number (i64) - current state in state machine
        # offset 16: done flag (i64) - 1 if generator is exhausted
        # offset 24: current_value (i64) - last yielded value
        # offset 32+: params (8 bytes each)
        # offset 32+num_params*8: locals (8 bytes each)
        base_size = 32
        params_offset = base_size
        locals_offset = params_offset + num_params * 8
        struct_size = locals_offset + num_locals * 8

        # Generate the constructor function that creates the generator/stream
        params_str = ', '.join(f"i64 %param.{p['name']}" for p in params)
        self.emit(f'; Generator function {name} - {num_states} states, {len(yield_points)} yield points')
        self.emit(f'define i64 @"{name}"({params_str}) {{')
        self.emit('entry:')

        self.emit(f'  %gen = call ptr @malloc(i64 {struct_size})')

        # Store next function pointer at offset 0
        self.emit(f'  %next_fn_ptr = ptrtoint ptr @"{name}_next" to i64')
        self.emit(f'  store i64 %next_fn_ptr, ptr %gen')

        # Store initial state (0) at offset 8
        self.emit(f'  %state_ptr = getelementptr i8, ptr %gen, i64 8')
        self.emit(f'  store i64 0, ptr %state_ptr')

        # Initialize done flag to 0 at offset 16
        self.emit(f'  %done_ptr = getelementptr i8, ptr %gen, i64 16')
        self.emit(f'  store i64 0, ptr %done_ptr')

        # Initialize current_value to 0 at offset 24
        self.emit(f'  %val_ptr = getelementptr i8, ptr %gen, i64 24')
        self.emit(f'  store i64 0, ptr %val_ptr')

        # Store parameters in generator struct
        for i, p in enumerate(params):
            offset = params_offset + i * 8
            self.emit(f'  %param_ptr_{i} = getelementptr i8, ptr %gen, i64 {offset}')
            self.emit(f'  store i64 %param.{p["name"]}, ptr %param_ptr_{i}')

        # Initialize locals to 0
        for i in range(num_locals):
            offset = locals_offset + i * 8
            self.emit(f'  %local_init_ptr_{i} = getelementptr i8, ptr %gen, i64 {offset}')
            self.emit(f'  store i64 0, ptr %local_init_ptr_{i}')

        self.emit(f'  %result = ptrtoint ptr %gen to i64')
        self.emit(f'  ret i64 %result')
        self.emit('}')
        self.emit('')

        # Generate the next function (iterator protocol)
        # Returns Option<T>: Some(value) = (value << 1) | 1, None = 0
        self.locals = {}
        self.temp_counter = 0
        self.label_counter = 0
        self.local_counter = 0
        self.pre_alloca_queue = []
        self.gen_yield_counter = 0
        self.gen_locals_offset = locals_offset
        self.gen_all_locals = all_locals

        self.emit(f'define i64 @"{name}_next"(i64 %gen_ptr) {{')
        self.emit('entry:')
        self.emit('  %gen = inttoptr i64 %gen_ptr to ptr')

        # Check if done
        self.emit('  %done_ptr = getelementptr i8, ptr %gen, i64 16')
        self.emit('  %done = load i64, ptr %done_ptr')
        self.emit('  %is_done = icmp ne i64 %done, 0')
        self.emit('  br i1 %is_done, label %exhausted, label %continue')

        self.emit('exhausted:')
        self.emit('  ret i64 0  ; None - generator exhausted')

        self.emit('continue:')

        # Load current state from offset 8
        self.emit('  %state_ptr = getelementptr i8, ptr %gen, i64 8')
        self.emit('  %state = load i64, ptr %state_ptr')

        # Allocate stack space for parameters and locals
        for i, p in enumerate(params):
            offset = params_offset + i * 8
            local = self.new_local(p['name'])
            self.emit(f'  {local} = alloca i64')
            self.emit(f'  %param_load_ptr_{i} = getelementptr i8, ptr %gen, i64 {offset}')
            self.emit(f'  %param_val_{i} = load i64, ptr %param_load_ptr_{i}')
            self.emit(f'  store i64 %param_val_{i}, ptr {local}')
            self.locals[p['name']] = local

        # Pre-allocate locals and load their saved values
        for i, local_name in enumerate(all_locals):
            local = self.new_local(local_name)
            self.emit(f'  {local} = alloca i64')
            offset = locals_offset + i * 8
            self.emit(f'  %local_load_ptr_{i} = getelementptr i8, ptr %gen, i64 {offset}')
            self.emit(f'  %local_val_{i} = load i64, ptr %local_load_ptr_{i}')
            self.emit(f'  store i64 %local_val_{i}, ptr {local}')
            self.pre_alloca_queue.append(local)

        # Generate state machine switch
        state_labels = [f'gen_state_{i}' for i in range(num_states)]
        default_label = 'gen_state_invalid'

        if num_states > 1:
            self.emit(f'  switch i64 %state, label %{default_label} [')
            for i in range(num_states):
                self.emit(f'    i64 {i}, label %{state_labels[i]}')
            self.emit('  ]')
            self.emit(f'{default_label}:')
            self.emit('  ret i64 0  ; Invalid state returns None')
        else:
            self.emit(f'  br label %{state_labels[0]}')

        # Generate state 0 - initial execution
        self.emit(f'{state_labels[0]}:')

        self.current_yield_index = 0
        self.gen_state_labels = state_labels
        self.num_gen_states = num_states

        result = self.generate_block(fn['body'])

        # Save locals back to generator struct
        for i, local_name in enumerate(all_locals):
            if local_name in self.locals:
                offset = locals_offset + i * 8
                local = self.locals[local_name]
                save_val = self.new_temp()
                self.emit(f'  {save_val} = load i64, ptr {local}')
                save_ptr = self.new_temp()
                self.emit(f'  {save_ptr} = getelementptr i8, ptr %gen, i64 {offset}')
                self.emit(f'  store i64 {save_val}, ptr {save_ptr}')

        # Mark as done and return None
        self.emit('  %final_done_ptr = getelementptr i8, ptr %gen, i64 16')
        self.emit('  store i64 1, ptr %final_done_ptr')
        self.emit('  ret i64 0  ; None - generator finished')

        self.emit('}')
        self.emit('')

        self.is_in_gen_fn = False
        # Clean up generator-specific state
        if hasattr(self, 'gen_yield_counter'):
            del self.gen_yield_counter
        if hasattr(self, 'gen_state_labels'):
            del self.gen_state_labels

    def generate_fn(self, fn):
        # Track function name for function pointer references
        self.functions.add(fn['name'])

        # Handle async functions
        if fn.get('is_async', False):
            return self.generate_async_fn(fn)

        self.locals = {}
        self.var_types = {}  # Clear for each function
        self.temp_counter = 0
        self.label_counter = 0
        self.local_counter = 0
        self.pre_alloca_queue = []  # Queue of pre-allocated locals in order
        self.current_type_params = set(fn.get('type_params', []))  # Track generic type params

        ret_type = self.type_to_llvm(fn['return_type'])

        # Special case: main() without return type should return i64 (exit code 0)
        is_main = fn['name'] == 'main'
        main_implicit_return = is_main and ret_type == 'void'
        if main_implicit_return:
            ret_type = 'i64'

        self.current_fn_return_type = ret_type

        # Build param list
        params = fn['params']
        params_str = ', '.join(f"{self.type_to_llvm(p['ty'])} %param.{p['name']}" for p in params)

        fn_name = 'simplex_main' if is_main else fn['name']
        self.emit(f'define {ret_type} @"{fn_name}"({params_str}) {{')
        self.emit('entry:')

        # Pre-allocate ALL local variables in the entry block to avoid
        # allocas in conditional blocks/loops which can cause stack corruption
        all_locals = self.collect_locals(fn['body'])
        for local_name in all_locals:
            local = self.new_local(local_name)
            self.emit(f'  {local} = alloca i64')
            self.pre_alloca_queue.append(local)

        # Reset local_counter for actual generation (params will use new numbers)
        # Actually, keep counter going to avoid conflicts

        # Allocate space for params and copy them
        for p in params:
            local = self.new_local(p['name'])
            self.emit(f'  {local} = alloca i64')
            self.emit(f'  store i64 %param.{p["name"]}, ptr {local}')
            self.locals[p['name']] = local

        result = self.generate_block(fn['body'])

        # Emit final return if needed (may be unreachable after returns in block)
        if main_implicit_return:
            # main() without explicit return type returns 0
            self.emit('  ret i64 0')
        elif ret_type == 'void':
            self.emit('  ret void')
        else:
            self.emit(f'  ret {ret_type} {result}')

        self.emit('}')
        self.emit('')

    def generate_impl(self, impl_def):
        type_name = impl_def['type_name']

        # Register associated types from this impl
        if impl_def.get('assoc_types'):
            for assoc_name, assoc_ty in impl_def['assoc_types'].items():
                self.assoc_types[(type_name, assoc_name)] = assoc_ty

        # Set current impl type for Self resolution
        self.current_impl_type = type_name

        for method in impl_def['methods']:
            # Mangle the method name: TypeName_methodName
            original_name = method['name']
            method['name'] = f"{type_name}_{original_name}"
            self.generate_fn(method)
            # Restore original name in case AST is reused
            method['name'] = original_name

        # Clear current impl type
        self.current_impl_type = None

    def generate_vtable(self, trait_name, type_name, impl_def):
        """Generate a vtable for a trait implementation.

        Vtable layout:
        - offset 0: type_id (for runtime type checking)
        - offset 8: drop function pointer
        - offset 16+: method pointers in trait method order
        """
        if trait_name not in self.traits:
            return  # Trait not defined, skip

        trait_def = self.traits[trait_name]
        trait_methods = trait_def.get('methods', [])

        # Create vtable global name
        vtable_name = f"__vtable_{trait_name}_{type_name}"
        self.vtables[(trait_name, type_name)] = vtable_name

        # Calculate vtable size: type_id + drop + methods
        num_methods = len(trait_methods)
        vtable_size = (2 + num_methods) * 8  # type_id, drop, then methods

        # Generate vtable as a global constant
        self.emit(f'; Vtable for {type_name} impl {trait_name}')
        self.emit(f'@"{vtable_name}" = private constant [{2 + num_methods} x i64] [')

        # Type ID (hash of type name for simplicity)
        type_id = hash(type_name) & 0xFFFFFFFFFFFFFFFF
        self.emit(f'  i64 {type_id},  ; type_id for {type_name}')

        # Drop function (if exists, else null)
        drop_fn = f'{type_name}_drop'
        if any(m['name'] == 'drop' for m in impl_def.get('methods', [])):
            self.emit(f'  i64 ptrtoint (ptr @"{drop_fn}" to i64),  ; drop')
        else:
            self.emit(f'  i64 0,  ; no drop')

        # Method pointers in order
        impl_methods = {m['name']: m for m in impl_def.get('methods', [])}
        for i, trait_method in enumerate(trait_methods):
            method_name = trait_method['name']
            mangled_name = f'{type_name}_{method_name}'
            if method_name in impl_methods:
                comma = '' if i == len(trait_methods) - 1 else ','
                self.emit(f'  i64 ptrtoint (ptr @"{mangled_name}" to i64){comma}  ; {method_name}')
            else:
                # Method not implemented - use default if available
                comma = '' if i == len(trait_methods) - 1 else ','
                default_name = f'{trait_name}_default_{method_name}'
                self.emit(f'  i64 0{comma}  ; {method_name} (not implemented)')

        self.emit(']')
        self.emit('')

    def resolve_trait_method(self, trait_name, type_name, method_name):
        """Resolve a trait method call to the concrete implementation."""
        # First check if there's a concrete implementation
        impl_key = (trait_name, type_name)
        if impl_key in self.trait_impls:
            return f'{type_name}_{method_name}'
        # Check if it's a generic bound that needs vtable dispatch
        return None

    def generate_dyn_dispatch(self, trait_name, method_name, obj_val, args):
        """Generate dynamic dispatch through vtable for dyn Trait."""
        # dyn Trait object layout: data_ptr(0), vtable_ptr(8)

        # Get trait method index
        if trait_name not in self.traits:
            return '0'

        trait_def = self.traits[trait_name]
        trait_methods = trait_def.get('methods', [])
        method_idx = None
        for i, m in enumerate(trait_methods):
            if m['name'] == method_name:
                method_idx = i
                break

        if method_idx is None:
            return '0'

        # Load data pointer and vtable pointer from dyn object
        obj_ptr = self.new_temp()
        self.emit(f'  {obj_ptr} = inttoptr i64 {obj_val} to ptr')

        data_ptr = self.new_temp()
        self.emit(f'  {data_ptr} = load i64, ptr {obj_ptr}')

        vtable_gep = self.new_temp()
        self.emit(f'  {vtable_gep} = getelementptr i8, ptr {obj_ptr}, i64 8')
        vtable_ptr = self.new_temp()
        self.emit(f'  {vtable_ptr} = load i64, ptr {vtable_gep}')

        # Load method from vtable (offset = (2 + method_idx) * 8)
        vtable_as_ptr = self.new_temp()
        self.emit(f'  {vtable_as_ptr} = inttoptr i64 {vtable_ptr} to ptr')

        method_offset = (2 + method_idx) * 8
        method_gep = self.new_temp()
        self.emit(f'  {method_gep} = getelementptr i8, ptr {vtable_as_ptr}, i64 {method_offset}')
        method_fn = self.new_temp()
        self.emit(f'  {method_fn} = load i64, ptr {method_gep}')

        # Call method with data_ptr as self, then remaining args
        method_fn_ptr = self.new_temp()
        self.emit(f'  {method_fn_ptr} = inttoptr i64 {method_fn} to ptr')

        # Build call args
        all_args = [data_ptr] + args
        args_str = ', '.join(f'i64 {a}' for a in all_args)

        result = self.new_temp()
        self.emit(f'  {result} = call i64 {method_fn_ptr}({args_str})')
        return result

    def generate_actor(self, actor_def):
        """Generate code for an actor definition.
        Generates:
        - ActorName_new() constructor
        - ActorName_handle_MessageName(self, args...) for each receive handler
        """
        actor_name = actor_def['name']
        state_vars = actor_def['state_vars']
        handlers = actor_def['handlers']
        num_fields = len(state_vars)

        # Generate constructor: ActorName_new() -> ptr as i64
        self.emit(f'; Actor {actor_name} constructor')
        self.emit(f'define i64 @"{actor_name}_new"() {{')
        self.emit('entry:')

        # Allocate actor struct
        alloc_size = max(num_fields * 8, 8)  # At least 8 bytes
        ptr_temp = self.new_temp()
        self.emit(f'  {ptr_temp} = call ptr @malloc(i64 {alloc_size})')

        # Initialize state variables
        for i, var in enumerate(state_vars):
            if var.get('init'):
                # Reset state for init expression generation
                saved_locals = self.locals
                saved_temp = self.temp_counter
                self.locals = {}
                self.temp_counter = 0
                self.pre_alloca_queue = []

                init_val = self.generate_expr(var['init'])

                self.locals = saved_locals
                self.temp_counter = saved_temp
            else:
                init_val = '0'

            offset = i * 8
            gep_temp = self.new_temp()
            self.emit(f'  {gep_temp} = getelementptr i8, ptr {ptr_temp}, i64 {offset}')
            self.emit(f'  store i64 {init_val}, ptr {gep_temp}')

        # Return pointer as i64
        result = self.new_temp()
        self.emit(f'  {result} = ptrtoint ptr {ptr_temp} to i64')
        self.emit(f'  ret i64 {result}')
        self.emit('}')
        self.emit('')

        # Generate handler for each receive
        for handler in handlers:
            msg_name = handler['name']
            params = handler['params']
            return_type = handler['return_type']
            body = handler['body']

            # Build param list: self + handler params
            all_params = [make_param('self', 'i64')] + params
            params_str = ', '.join(f"i64 %param.{p['name']}" for p in all_params)
            ret_type = self.type_to_llvm(return_type)

            self.emit(f'; Actor {actor_name} handler: {msg_name}')
            self.emit(f'define {ret_type} @"{actor_name}_handle_{msg_name}"({params_str}) {{')
            self.emit('entry:')

            # Reset state for handler generation
            self.locals = {}
            self.var_types = {}
            self.temp_counter = 0
            self.label_counter = 0
            self.local_counter = 0
            self.pre_alloca_queue = []
            self.current_fn_return_type = ret_type
            self.current_actor = actor_name
            self.current_actor_self = None

            # Pre-allocate locals
            all_locals = self.collect_locals(body)
            for local_name in all_locals:
                local = self.new_local(local_name)
                self.emit(f'  {local} = alloca i64')
                self.pre_alloca_queue.append(local)

            # Allocate space for params
            for p in all_params:
                local = self.new_local(p['name'])
                self.emit(f'  {local} = alloca i64')
                self.emit(f'  store i64 %param.{p["name"]}, ptr {local}')
                self.locals[p['name']] = local
                if p['name'] == 'self':
                    self.current_actor_self = local

            result = self.generate_block(body)

            if ret_type == 'void':
                self.emit('  ret void')
            else:
                self.emit(f'  ret {ret_type} {result}')

            self.emit('}')
            self.emit('')

            self.current_actor = None
            self.current_actor_self = None

    def generate_specialist(self, specialist_def):
        """Generate code for a specialist definition.
        Specialist is like actor but with model config and infer() available.
        Generates same code as actor but stores model config.
        """
        spec_name = specialist_def['name']
        config = specialist_def['config']
        state_vars = specialist_def['state_vars']
        handlers = specialist_def['handlers']

        # Store model config as extra state vars
        model_name = config.get('model', 'default')
        temp_val = config.get('temperature', 70)  # Store as integer * 100

        # Add config to state vars
        all_state_vars = [
            {'name': '__model', 'ty': 'String', 'init': make_string_expr(model_name)},
            {'name': '__temperature', 'ty': 'i64', 'init': make_int_expr(temp_val)}
        ] + state_vars
        num_fields = len(all_state_vars)

        # Generate constructor
        self.emit(f'; Specialist {spec_name} constructor')
        self.emit(f'define i64 @"{spec_name}_new"() {{')
        self.emit('entry:')

        alloc_size = max(num_fields * 8, 8)
        ptr_temp = self.new_temp()
        self.emit(f'  {ptr_temp} = call ptr @malloc(i64 {alloc_size})')

        # Initialize state variables
        for i, var in enumerate(all_state_vars):
            if var.get('init'):
                # Generate init expression in same context
                init_val = self.generate_expr(var['init'])
            else:
                init_val = '0'

            offset = i * 8
            gep_temp = self.new_temp()
            self.emit(f'  {gep_temp} = getelementptr i8, ptr {ptr_temp}, i64 {offset}')
            self.emit(f'  store i64 {init_val}, ptr {gep_temp}')

        result = self.new_temp()
        self.emit(f'  {result} = ptrtoint ptr {ptr_temp} to i64')
        self.emit(f'  ret i64 {result}')
        self.emit('}')
        self.emit('')

        # Generate handlers (same as actor but with current_specialist set)
        for handler in handlers:
            msg_name = handler['name']
            params = handler['params']
            return_type = handler['return_type']
            body = handler['body']

            all_params = [make_param('self', 'i64')] + params
            params_str = ', '.join(f"i64 %param.{p['name']}" for p in all_params)
            ret_type = self.type_to_llvm(return_type)

            # Specialist handlers always return i64 for consistency
            handler_ret_type = 'i64' if ret_type != 'void' else 'void'
            self.emit(f'; Specialist {spec_name} handler: {msg_name}')
            self.emit(f'define {handler_ret_type} @"{spec_name}_handle_{msg_name}"({params_str}) {{')
            self.emit('entry:')

            self.locals = {}
            self.var_types = {}
            self.temp_counter = 0
            self.label_counter = 0
            self.local_counter = 0
            self.pre_alloca_queue = []
            self.current_fn_return_type = ret_type
            self.current_actor = spec_name
            self.current_specialist = spec_name

            all_locals = self.collect_locals(body)
            for local_name in all_locals:
                local = self.new_local(local_name)
                self.emit(f'  {local} = alloca i64')
                self.pre_alloca_queue.append(local)

            for p in all_params:
                local = self.new_local(p['name'])
                self.emit(f'  {local} = alloca i64')
                self.emit(f'  store i64 %param.{p["name"]}, ptr {local}')
                self.locals[p['name']] = local

            result = self.generate_block(body)

            if handler_ret_type == 'void':
                self.emit('  ret void')
            else:
                self.emit(f'  ret {handler_ret_type} {result}')

            self.emit('}')
            self.emit('')

            self.current_actor = None
            self.current_specialist = None

    def generate_hive(self, hive_def):
        """Generate code for a hive definition.
        Hive is a supervisor that spawns and manages specialists.
        """
        hive_name = hive_def['name']
        specialists = hive_def['specialists']
        router = hive_def['router']
        strategy = hive_def['strategy']

        num_specs = len(specialists)
        struct_size = max(num_specs * 8, 8)

        # Generate constructor that spawns all specialists
        self.emit(f'; Hive {hive_name} constructor')
        self.emit(f'define i64 @"{hive_name}_new"() {{')
        self.emit('entry:')

        ptr_temp = self.new_temp()
        self.emit(f'  {ptr_temp} = call ptr @malloc(i64 {struct_size})')

        for i, spec in enumerate(specialists):
            # Spawn each specialist
            spec_ptr = self.new_temp()
            self.emit(f'  {spec_ptr} = call i64 @"{spec}_new"()')

            # Store in hive struct
            offset = i * 8
            gep_temp = self.new_temp()
            self.emit(f'  {gep_temp} = getelementptr i8, ptr {ptr_temp}, i64 {offset}')
            self.emit(f'  store i64 {spec_ptr}, ptr {gep_temp}')

        result = self.new_temp()
        self.emit(f'  {result} = ptrtoint ptr {ptr_temp} to i64')
        self.emit(f'  ret i64 {result}')
        self.emit('}')
        self.emit('')

    def generate_block(self, block):
        result = '0'
        for stmt in block['stmts']:
            self.generate_stmt(stmt)
        if block['expr']:
            result = self.generate_expr(block['expr'])
        return result

    def generate_stmt(self, stmt):
        if stmt['type'] == 'LetStmt':
            value = self.generate_expr(stmt['value'])
            name = stmt['name']
            ty = stmt.get('ty')
            # Infer type from struct literal if no explicit type
            if not ty and stmt['value'].get('type') == 'StructLit':
                ty = stmt['value'].get('name')
            # Use pre-allocated local from entry block (in order)
            if self.pre_alloca_queue:
                local = self.pre_alloca_queue.pop(0)
            else:
                # Fallback: allocate inline (shouldn't happen with proper pre-pass)
                local = self.new_local(name)
                self.emit(f'  {local} = alloca i64')
            self.emit(f'  store i64 {value}, ptr {local}')
            self.locals[name] = local
            # Record type for method call resolution
            if ty:
                self.var_types[name] = ty
        elif stmt['type'] == 'AssignStmt':
            value = self.generate_expr(stmt['value'])
            name = stmt['name']
            if name in self.locals:
                local = self.locals[name]
                self.emit(f'  store i64 {value}, ptr {local}')
            elif hasattr(self, 'current_actor') and self.current_actor and self.current_actor in self.actors:
                # Check if assigning to actor state variable
                actor_def = self.actors[self.current_actor]
                found = False
                for i, var in enumerate(actor_def['state_vars']):
                    if var['name'] == name:
                        # Store to actor's state
                        self_local = self.locals.get('self')
                        if self_local:
                            self_ptr = self.new_temp()
                            self.emit(f'  {self_ptr} = load i64, ptr {self_local}')
                            ptr_temp = self.new_temp()
                            self.emit(f'  {ptr_temp} = inttoptr i64 {self_ptr} to ptr')
                            offset = i * 8
                            gep_temp = self.new_temp()
                            self.emit(f'  {gep_temp} = getelementptr i8, ptr {ptr_temp}, i64 {offset}')
                            self.emit(f'  store i64 {value}, ptr {gep_temp}')
                            found = True
                            break
                if not found:
                    raise RuntimeError(f"Undefined variable: {name}")
            else:
                raise RuntimeError(f"Undefined variable: {name}")
        elif stmt['type'] == 'DerefAssignStmt':
            # *ptr = value
            target = stmt['target']
            value = self.generate_expr(stmt['value'])
            ptr_val = self.generate_expr(target['operand'])
            temp = self.new_temp()
            self.emit(f'  {temp} = inttoptr i64 {ptr_val} to ptr')
            self.emit(f'  store i64 {value}, ptr {temp}')
        elif stmt['type'] == 'FieldAssignStmt':
            # obj.field = value
            target = stmt['target']
            value = self.generate_expr(stmt['value'])
            obj = target['object']
            field_name = target['field']
            # Get object pointer
            if obj['type'] == 'IdentExpr':
                obj_name = obj['name']
                if obj_name == 'self' and hasattr(self, 'current_impl_type') and self.current_impl_type:
                    # self.field = value in struct method
                    self_local = self.locals.get('self')
                    if self_local:
                        struct_name = self.current_impl_type
                        struct_fields = self.structs.get(struct_name, [])
                        for i, (fname, _) in enumerate(struct_fields):
                            if fname == field_name:
                                self_ptr = self.new_temp()
                                self.emit(f'  {self_ptr} = load i64, ptr {self_local}')
                                ptr_temp = self.new_temp()
                                self.emit(f'  {ptr_temp} = inttoptr i64 {self_ptr} to ptr')
                                offset = i * 8
                                gep_temp = self.new_temp()
                                self.emit(f'  {gep_temp} = getelementptr i8, ptr {ptr_temp}, i64 {offset}')
                                self.emit(f'  store i64 {value}, ptr {gep_temp}')
                                break
                elif obj_name in self.locals:
                    # Regular struct field assignment
                    local = self.locals[obj_name]
                    struct_type = self.var_types.get(obj_name)
                    struct_fields = self.structs.get(struct_type, []) if struct_type else []
                    for i, (fname, _) in enumerate(struct_fields):
                        if fname == field_name:
                            obj_ptr = self.new_temp()
                            self.emit(f'  {obj_ptr} = load i64, ptr {local}')
                            ptr_temp = self.new_temp()
                            self.emit(f'  {ptr_temp} = inttoptr i64 {obj_ptr} to ptr')
                            offset = i * 8
                            gep_temp = self.new_temp()
                            self.emit(f'  {gep_temp} = getelementptr i8, ptr {ptr_temp}, i64 {offset}')
                            self.emit(f'  store i64 {value}, ptr {gep_temp}')
                            break
        elif stmt['type'] == 'ReturnStmt':
            if stmt['value']:
                value = self.generate_expr(stmt['value'])
                self.emit(f'  ret {self.current_fn_return_type} {value}')
            else:
                self.emit('  ret void')
        elif stmt['type'] == 'ExprStmt':
            self.generate_expr(stmt['expr'])
        elif stmt['type'] == 'BreakStmt':
            if self.loop_stack:
                _, break_label = self.loop_stack[-1]
                self.emit(f'  br label %{break_label}')
                # Create unreachable block to avoid LLVM errors
                unreachable_label = self.new_label('after_break')
                self.emit(f'{unreachable_label}:')
            else:
                raise RuntimeError("break outside of loop")
        elif stmt['type'] == 'ContinueStmt':
            if self.loop_stack:
                continue_label, _ = self.loop_stack[-1]
                self.emit(f'  br label %{continue_label}')
                # Create unreachable block to avoid LLVM errors
                unreachable_label = self.new_label('after_continue')
                self.emit(f'{unreachable_label}:')
            else:
                raise RuntimeError("continue outside of loop")

    def generate_format_macro(self, args):
        """Generate code for format!("template {}", arg1, arg2, ...) macro.
        Parses format string and interpolates arguments."""
        if not args:
            return '0'

        # First arg should be the format string
        fmt_arg = args[0]
        if fmt_arg.get('type') != 'StringExpr':
            # Not a literal string, just call format intrinsic
            fmt_val = self.generate_expr(fmt_arg)
            return fmt_val

        fmt_str = fmt_arg['value']
        remaining_args = args[1:]

        # Parse format string and build result
        result = None
        arg_idx = 0
        i = 0

        while i < len(fmt_str):
            if fmt_str[i] == '{':
                if i + 1 < len(fmt_str) and fmt_str[i + 1] == '{':
                    # Escaped {{ -> {
                    literal = "{"
                    label = self.add_string_constant(literal)
                    lit_temp = self.new_temp()
                    self.emit(f'  {lit_temp} = ptrtoint ptr {label} to i64')
                    if result is None:
                        result = lit_temp
                    else:
                        concat = self.new_temp()
                        self.emit(f'  {concat} = call i64 @intrinsic_string_concat(i64 {result}, i64 {lit_temp})')
                        result = concat
                    i += 2
                    continue

                # Find matching }
                end = fmt_str.find('}', i)
                if end == -1:
                    end = len(fmt_str)

                # Extract format spec (between { and })
                spec = fmt_str[i+1:end]

                # Generate argument value
                if arg_idx < len(remaining_args):
                    arg_val = self.generate_expr(remaining_args[arg_idx])
                    arg_idx += 1

                    # Convert to string based on format specifier
                    if ':d' in spec or spec == '' or spec.isdigit():
                        # Integer format (default)
                        str_val = self.new_temp()
                        self.emit(f'  {str_val} = call i64 @intrinsic_int_to_string(i64 {arg_val})')
                    elif ':s' in spec:
                        # String format (already a string)
                        str_val = arg_val
                    elif ':x' in spec:
                        # Hex format
                        str_val = self.new_temp()
                        self.emit(f'  {str_val} = call i64 @intrinsic_int_to_hex(i64 {arg_val})')
                    elif ':b' in spec:
                        # Binary format
                        str_val = self.new_temp()
                        self.emit(f'  {str_val} = call i64 @intrinsic_int_to_binary(i64 {arg_val})')
                    elif ':?' in spec:
                        # Debug format (calls Debug trait)
                        str_val = self.new_temp()
                        self.emit(f'  {str_val} = call i64 @debug_format(i64 {arg_val})')
                    else:
                        # Default: try Display trait, fallback to int_to_string
                        str_val = self.new_temp()
                        self.emit(f'  {str_val} = call i64 @display_format(i64 {arg_val})')

                    if result is None:
                        result = str_val
                    else:
                        concat = self.new_temp()
                        self.emit(f'  {concat} = call i64 @intrinsic_string_concat(i64 {result}, i64 {str_val})')
                        result = concat

                i = end + 1
            elif fmt_str[i] == '}':
                if i + 1 < len(fmt_str) and fmt_str[i + 1] == '}':
                    # Escaped }} -> }
                    literal = "}"
                    label = self.add_string_constant(literal)
                    lit_temp = self.new_temp()
                    self.emit(f'  {lit_temp} = ptrtoint ptr {label} to i64')
                    if result is None:
                        result = lit_temp
                    else:
                        concat = self.new_temp()
                        self.emit(f'  {concat} = call i64 @intrinsic_string_concat(i64 {result}, i64 {lit_temp})')
                        result = concat
                    i += 2
                    continue
                i += 1
            else:
                # Literal text
                start = i
                while i < len(fmt_str) and fmt_str[i] not in '{}':
                    i += 1
                literal = fmt_str[start:i]
                if literal:
                    label = self.add_string_constant(literal)
                    lit_temp = self.new_temp()
                    self.emit(f'  {lit_temp} = ptrtoint ptr {label} to i64')
                    if result is None:
                        result = lit_temp
                    else:
                        concat = self.new_temp()
                        self.emit(f'  {concat} = call i64 @intrinsic_string_concat(i64 {result}, i64 {lit_temp})')
                        result = concat

        if result is None:
            # Empty format string
            label = self.add_string_constant("")
            result = self.new_temp()
            self.emit(f'  {result} = ptrtoint ptr {label} to i64')

        return result

    def generate_expr(self, expr):
        expr_type = expr['type']

        if expr_type == 'IntExpr':
            return str(expr['value'])

        if expr_type == 'FloatExpr':
            # Return float as LLVM double constant
            return f"0x{struct.pack('>d', expr['value']).hex().upper()}"

        if expr_type == 'BoolExpr':
            return '1' if expr['value'] else '0'

        if expr_type == 'StringExpr':
            value = expr['value']
            label = self.add_string_constant(value)
            temp = self.new_temp()
            self.emit(f'  {temp} = call ptr @intrinsic_string_new(ptr {label})')
            temp2 = self.new_temp()
            self.emit(f'  {temp2} = ptrtoint ptr {temp} to i64')
            return temp2

        if expr_type == 'FStringExpr':
            parts = expr['parts']
            exprs = expr['exprs']
            format_specs = expr.get('format_specs', [])
            result = '0'  # Start with empty/null
            for i, part in enumerate(parts):
                # Generate string part if non-empty
                if part:
                    label = self.add_string_constant(part)
                    t1 = self.new_temp()
                    self.emit(f'  {t1} = call ptr @intrinsic_string_new(ptr {label})')
                    t2 = self.new_temp()
                    self.emit(f'  {t2} = ptrtoint ptr {t1} to i64')
                    if result == '0':
                        result = t2
                    else:
                        tc = self.new_temp()
                        self.emit(f'  {tc} = call i64 @intrinsic_string_concat(i64 {result}, i64 {t2})')
                        result = tc
                # Generate expression if there is one
                if i < len(exprs):
                    ex_val = self.generate_expr(exprs[i])
                    # Check for format specifier
                    format_spec = format_specs[i] if i < len(format_specs) else None
                    if format_spec:
                        # Apply format specifier
                        # Common patterns: .2f (float precision), d (integer), s (string), x (hex)
                        if format_spec.endswith('f'):
                            # Float formatting: {x:.2f}
                            precision = 2  # default
                            if '.' in format_spec:
                                try:
                                    precision = int(format_spec[format_spec.index('.')+1:-1])
                                except:
                                    precision = 2
                            # Call f64 formatting with precision
                            formatted = self.new_temp()
                            self.emit(f'  {formatted} = call i64 @format_f64(i64 {ex_val}, i64 {precision})')
                            ex_val = formatted
                        elif format_spec == 'd':
                            # Integer formatting (default)
                            formatted = self.new_temp()
                            self.emit(f'  {formatted} = call i64 @intrinsic_int_to_string(i64 {ex_val})')
                            ex_val = formatted
                        elif format_spec == 'x':
                            # Hex formatting
                            formatted = self.new_temp()
                            self.emit(f'  {formatted} = call i64 @format_hex(i64 {ex_val})')
                            ex_val = formatted
                        elif format_spec == 'X':
                            # Uppercase hex formatting
                            formatted = self.new_temp()
                            self.emit(f'  {formatted} = call i64 @format_hex_upper(i64 {ex_val})')
                            ex_val = formatted
                        elif format_spec == 'b':
                            # Binary formatting
                            formatted = self.new_temp()
                            self.emit(f'  {formatted} = call i64 @format_binary(i64 {ex_val})')
                            ex_val = formatted
                        # Width/padding specifiers could be added here
                    if result == '0':
                        result = ex_val
                    else:
                        tc = self.new_temp()
                        self.emit(f'  {tc} = call i64 @intrinsic_string_concat(i64 {result}, i64 {ex_val})')
                        result = tc
            return result

        if expr_type == 'YieldExpr':
            inner = self.generate_expr(expr['inner'])

            # If we're in a generator function, handle state machine yield
            if getattr(self, 'is_in_gen_fn', False) and hasattr(self, 'gen_state_labels'):
                yield_idx = self.current_yield_index
                self.current_yield_index += 1
                next_state = yield_idx + 1

                # Save locals to generator struct before yielding
                if hasattr(self, 'gen_all_locals') and hasattr(self, 'gen_locals_offset'):
                    for i, local_name in enumerate(self.gen_all_locals):
                        if local_name in self.locals:
                            offset = self.gen_locals_offset + i * 8
                            local = self.locals[local_name]
                            save_val = self.new_temp()
                            self.emit(f'  {save_val} = load i64, ptr {local}')
                            save_ptr = self.new_temp()
                            self.emit(f'  {save_ptr} = getelementptr i8, ptr %gen, i64 {offset}')
                            self.emit(f'  store i64 {save_val}, ptr {save_ptr}')

                # Update state for next call
                self.emit(f'  %yield_state_ptr_{yield_idx} = getelementptr i8, ptr %gen, i64 8')
                self.emit(f'  store i64 {next_state}, ptr %yield_state_ptr_{yield_idx}')

                # Return Some(value) = (value << 1) | 1
                shifted = self.new_temp()
                self.emit(f'  {shifted} = shl i64 {inner}, 1')
                tagged = self.new_temp()
                self.emit(f'  {tagged} = or i64 {shifted}, 1')
                self.emit(f'  ret i64 {tagged}')

                # Emit label for resumption point
                if next_state < len(self.gen_state_labels):
                    self.emit(f'{self.gen_state_labels[next_state]}:')

                return '0'  # Placeholder, control doesn't reach here
            else:
                # Fallback: call generator_yield intrinsic
                temp = self.new_temp()
                self.emit(f'  {temp} = call i64 @generator_yield(i64 {inner})')
                return temp

        if expr_type == 'IdentExpr':
            name = expr['name']
            # Check if this is a const generic parameter
            if name in self.const_params:
                return self.const_params[name]
            # Check if this is an enum variant without arguments (like None, Err)
            for enum_name, variants in self.enums.items():
                if name in variants:
                    disc_val = variants[name]
                    # Construct enum with just the discriminant (no payload)
                    ptr_temp = self.new_temp()
                    self.emit(f'  {ptr_temp} = call ptr @malloc(i64 8)')
                    self.emit(f'  store i64 {disc_val}, ptr {ptr_temp}')
                    result_temp = self.new_temp()
                    self.emit(f'  {result_temp} = ptrtoint ptr {ptr_temp} to i64')
                    return result_temp
            if name in self.locals:
                local = self.locals[name]
                temp = self.new_temp()
                self.emit(f'  {temp} = load i64, ptr {local}')
                return temp
            # Check if we're inside an actor handler and this is a state variable
            if hasattr(self, 'current_actor') and self.current_actor and self.current_actor in self.actors:
                actor_def = self.actors[self.current_actor]
                for i, var in enumerate(actor_def['state_vars']):
                    if var['name'] == name:
                        # Load from actor's state (self is pointer to actor struct)
                        self_local = self.locals.get('self')
                        if self_local:
                            self_ptr = self.new_temp()
                            self.emit(f'  {self_ptr} = load i64, ptr {self_local}')
                            ptr_temp = self.new_temp()
                            self.emit(f'  {ptr_temp} = inttoptr i64 {self_ptr} to ptr')
                            offset = i * 8
                            gep_temp = self.new_temp()
                            self.emit(f'  {gep_temp} = getelementptr i8, ptr {ptr_temp}, i64 {offset}')
                            load_temp = self.new_temp()
                            self.emit(f'  {load_temp} = load i64, ptr {gep_temp}')
                            return load_temp
            # Check if this is a function reference (for function pointers)
            if name in self.functions:
                # Return function pointer as i64
                temp = self.new_temp()
                self.emit(f'  {temp} = ptrtoint ptr @"{name}" to i64')
                return temp
            return f'%{name}'

        # P1.1: Address-of operator (&x)
        if expr_type == 'AddressOfExpr':
            operand = expr['operand']
            # For &ident, get the address of the local variable
            if operand['type'] == 'IdentExpr':
                name = operand['name']
                if name in self.locals:
                    local = self.locals[name]
                    temp = self.new_temp()
                    self.emit(f'  {temp} = ptrtoint ptr {local} to i64')
                    return temp
            # For other cases, generate the operand and return as-is (already a pointer)
            val = self.generate_expr(operand)
            return val

        # P1.2: Dereference operator (*p)
        if expr_type == 'DerefExpr':
            operand = expr['operand']
            ptr_val = self.generate_expr(operand)
            temp1 = self.new_temp()
            self.emit(f'  {temp1} = inttoptr i64 {ptr_val} to ptr')
            temp2 = self.new_temp()
            self.emit(f'  {temp2} = load i64, ptr {temp1}')
            return temp2

        if expr_type == 'EnumVariantExpr':
            enum_name = expr['enum_name']
            if enum_name in self.enums:
                return str(self.enums[enum_name].get(expr['variant'], 0))
            return '0'

        if expr_type == 'BinaryExpr':
            left = self.generate_expr(expr['left'])
            right = self.generate_expr(expr['right'])
            temp = self.new_temp()

            op = expr['op']
            if op == '+':
                self.emit(f'  {temp} = add i64 {left}, {right}')
            elif op == '-':
                self.emit(f'  {temp} = sub i64 {left}, {right}')
            elif op == '*':
                self.emit(f'  {temp} = mul i64 {left}, {right}')
            elif op == '/':
                self.emit(f'  {temp} = sdiv i64 {left}, {right}')
            elif op == '%':
                self.emit(f'  {temp} = srem i64 {left}, {right}')
            elif op == '&':
                self.emit(f'  {temp} = and i64 {left}, {right}')
            elif op == '|':
                self.emit(f'  {temp} = or i64 {left}, {right}')
            elif op == '^':
                self.emit(f'  {temp} = xor i64 {left}, {right}')
            elif op == '<<':
                self.emit(f'  {temp} = shl i64 {left}, {right}')
            elif op == '>>':
                self.emit(f'  {temp} = ashr i64 {left}, {right}')
            elif op == '==':
                self.emit(f'  {temp} = icmp eq i64 {left}, {right}')
                temp2 = self.new_temp()
                self.emit(f'  {temp2} = zext i1 {temp} to i64')
                return temp2
            elif op == '!=':
                self.emit(f'  {temp} = icmp ne i64 {left}, {right}')
                temp2 = self.new_temp()
                self.emit(f'  {temp2} = zext i1 {temp} to i64')
                return temp2
            elif op == '<':
                self.emit(f'  {temp} = icmp slt i64 {left}, {right}')
                temp2 = self.new_temp()
                self.emit(f'  {temp2} = zext i1 {temp} to i64')
                return temp2
            elif op == '>':
                self.emit(f'  {temp} = icmp sgt i64 {left}, {right}')
                temp2 = self.new_temp()
                self.emit(f'  {temp2} = zext i1 {temp} to i64')
                return temp2
            elif op == '<=':
                self.emit(f'  {temp} = icmp sle i64 {left}, {right}')
                temp2 = self.new_temp()
                self.emit(f'  {temp2} = zext i1 {temp} to i64')
                return temp2
            elif op == '>=':
                self.emit(f'  {temp} = icmp sge i64 {left}, {right}')
                temp2 = self.new_temp()
                self.emit(f'  {temp2} = zext i1 {temp} to i64')
                return temp2
            elif op == '&&':
                # Short-circuit: left && right = if left then right else 0
                self.emit(f'  {temp} = icmp ne i64 {left}, 0')
                temp2 = self.new_temp()
                self.emit(f'  {temp2} = icmp ne i64 {right}, 0')
                temp3 = self.new_temp()
                self.emit(f'  {temp3} = and i1 {temp}, {temp2}')
                temp4 = self.new_temp()
                self.emit(f'  {temp4} = zext i1 {temp3} to i64')
                return temp4
            elif op == '||':
                # Short-circuit: left || right = if left then 1 else right
                self.emit(f'  {temp} = icmp ne i64 {left}, 0')
                temp2 = self.new_temp()
                self.emit(f'  {temp2} = icmp ne i64 {right}, 0')
                temp3 = self.new_temp()
                self.emit(f'  {temp3} = or i1 {temp}, {temp2}')
                temp4 = self.new_temp()
                self.emit(f'  {temp4} = zext i1 {temp3} to i64')
                return temp4
            else:
                self.emit(f'  {temp} = add i64 {left}, {right}  ; unknown op {op}')
            return temp

        if expr_type == 'CallExpr':
            orig_func_name = expr['func']
            raw_args = expr['args']
            type_args = expr.get('type_args', [])

            # Handle enum variant constructors: Some(x), None, Ok(x), Err(e)
            # Check if this is an enum variant
            enum_variant_info = None
            for enum_name, variants in self.enums.items():
                if orig_func_name in variants:
                    enum_variant_info = (enum_name, orig_func_name, variants[orig_func_name])
                    break
                # Also check qualified names like Option::Some
                if '::' in orig_func_name:
                    parts = orig_func_name.split('::')
                    if parts[0] == enum_name and parts[1] in variants:
                        enum_variant_info = (enum_name, parts[1], variants[parts[1]])
                        break

            if enum_variant_info:
                enum_name, variant_name, disc_val = enum_variant_info
                # Generate enum construction
                # Allocate space: 8 bytes tag + 8 bytes per payload field
                num_payloads = len(raw_args)
                size = 8 + num_payloads * 8
                # Allocate
                ptr_temp = self.new_temp()
                self.emit(f'  {ptr_temp} = call ptr @malloc(i64 {size})')
                # Store discriminant at offset 0
                self.emit(f'  store i64 {disc_val}, ptr {ptr_temp}')
                # Store payload values at offset 8, 16, etc.
                for i, arg in enumerate(raw_args):
                    arg_val = self.generate_expr(arg)
                    offset = 8 + i * 8
                    if offset > 0:
                        gep_temp = self.new_temp()
                        self.emit(f'  {gep_temp} = getelementptr i8, ptr {ptr_temp}, i64 {offset}')
                        self.emit(f'  store i64 {arg_val}, ptr {gep_temp}')
                    else:
                        self.emit(f'  store i64 {arg_val}, ptr {ptr_temp}')
                # Return ptr as i64
                result_temp = self.new_temp()
                self.emit(f'  {result_temp} = ptrtoint ptr {ptr_temp} to i64')
                return result_temp

            # ===== BUILTIN STATIC METHOD CALLS =====
            # Handle Vec::new(), String::from(), etc.
            static_methods = {
                'Vec::new': ('intrinsic_vec_new', [], 'ptr'),
                'Vec_new': ('intrinsic_vec_new', [], 'ptr'),
                'String::new': ('intrinsic_string_new_empty', [], 'ptr'),
                'String_new': ('intrinsic_string_new_empty', [], 'ptr'),
                # String::from with string literal is handled specially below
            }

            if orig_func_name in static_methods:
                intrinsic_name, param_types, ret_type = static_methods[orig_func_name]
                result_temp = self.new_temp()
                if ret_type == 'ptr':
                    ptr_temp = self.new_temp()
                    self.emit(f'  {ptr_temp} = call ptr @{intrinsic_name}()')
                    self.emit(f'  {result_temp} = ptrtoint ptr {ptr_temp} to i64')
                else:
                    self.emit(f'  {result_temp} = call {ret_type} @{intrinsic_name}()')
                return result_temp

            # Handle format! macro: format!("template", args...) -> String
            if orig_func_name == 'format':
                return self.generate_format_macro(raw_args)

            # Handle print_i64: convert to string, then print
            if orig_func_name == 'print_i64' and len(raw_args) == 1:
                arg_val = self.generate_expr(raw_args[0])
                str_temp = self.new_temp()
                self.emit(f'  {str_temp} = call ptr @intrinsic_int_to_string(i64 {arg_val})')
                self.emit(f'  call void @intrinsic_print(ptr {str_temp})')
                return '0'

            # Handle print_string: just print the string
            if orig_func_name == 'print_string' and len(raw_args) == 1:
                arg_val = self.generate_expr(raw_args[0])
                ptr_temp = self.new_temp()
                self.emit(f'  {ptr_temp} = inttoptr i64 {arg_val} to ptr')
                self.emit(f'  call void @intrinsic_print(ptr {ptr_temp})')
                return '0'

            # Handle println! macro with format string
            if orig_func_name == 'println' and len(raw_args) >= 1:
                if raw_args[0].get('type') == 'StringExpr':
                    fmt_str = raw_args[0]['value']
                    if '{' in fmt_str:
                        # Format string with placeholders
                        result = self.generate_format_macro(raw_args)
                        newline = self.add_string_constant("\n")
                        concat_temp = self.new_temp()
                        self.emit(f'  {concat_temp} = call i64 @intrinsic_string_concat(i64 {result}, ptr {newline})')
                        self.emit(f'  call void @print_string(i64 {concat_temp})')
                        return '0'

            # Handle assert! and assert_eq! macros
            if orig_func_name == 'assert' and len(raw_args) == 1:
                cond = self.generate_expr(raw_args[0])
                fail_label = self.new_label('assert_fail')
                ok_label = self.new_label('assert_ok')
                cond_temp = self.new_temp()
                self.emit(f'  {cond_temp} = icmp ne i64 {cond}, 0')
                self.emit(f'  br i1 {cond_temp}, label %{ok_label}, label %{fail_label}')
                self.emit(f'{fail_label}:')
                self.emit('  call void @panic(ptr @str_assertion_failed)')
                self.emit('  unreachable')
                self.emit(f'{ok_label}:')
                return '0'

            if orig_func_name == 'assert_eq' and len(raw_args) == 2:
                left = self.generate_expr(raw_args[0])
                right = self.generate_expr(raw_args[1])
                fail_label = self.new_label('assert_eq_fail')
                ok_label = self.new_label('assert_eq_ok')
                cmp_temp = self.new_temp()
                self.emit(f'  {cmp_temp} = icmp eq i64 {left}, {right}')
                self.emit(f'  br i1 {cmp_temp}, label %{ok_label}, label %{fail_label}')
                self.emit(f'{fail_label}:')
                self.emit('  call void @panic(ptr @str_assertion_eq_failed)')
                self.emit('  unreachable')
                self.emit(f'{ok_label}:')
                return '0'

            # Handle generic function calls with type arguments
            func_name = orig_func_name
            if type_args and orig_func_name in self.generic_fns:
                func_name = self.queue_generic_instantiation(orig_func_name, type_args)

            # Special case: string_from/string_new/String::from with a string literal
            # should pass the raw C string pointer, not a wrapped String*
            if orig_func_name in ('string_from', 'string_new', 'String_from', 'String::from') and len(raw_args) == 1:
                arg0 = raw_args[0]
                if arg0.get('type') == 'StringExpr':
                    # Pass the raw string constant directly
                    value = arg0['value']
                    label = self.add_string_constant(value)
                    temp = self.new_temp()
                    self.emit(f'  {temp} = call ptr @intrinsic_string_new(ptr {label})')
                    temp2 = self.new_temp()
                    self.emit(f'  {temp2} = ptrtoint ptr {temp} to i64')
                    return temp2

            args = [self.generate_expr(a) for a in raw_args]
            temp = self.new_temp()

            # Check for intrinsics
            intrinsic_map = {
                'malloc': 'malloc',
                'free': 'free',
                'store_ptr': 'store_ptr',
                'store_i64': 'store_i64',
                'load_ptr': 'load_ptr',
                'load_i64': 'load_i64',
                'string_len': 'intrinsic_string_len',
                'string_char_at': 'intrinsic_string_char_at',
                'string_slice': 'intrinsic_string_slice',
                'string_eq': 'intrinsic_string_eq',
                'string_from': 'intrinsic_string_new',
                'string_from_char': 'intrinsic_string_from_char',
                'string_concat': 'intrinsic_string_concat',
                'vec_new': 'intrinsic_vec_new',
                'vec_push': 'intrinsic_vec_push',
                'vec_get': 'intrinsic_vec_get',
                'vec_len': 'intrinsic_vec_len',
                'println': 'intrinsic_println',
                'print': 'intrinsic_print',
                'int_to_string': 'intrinsic_int_to_string',
                'string_to_int': 'intrinsic_string_to_int',
                'get_args': 'intrinsic_get_args',
                'read_file': 'intrinsic_read_file',
                'write_file': 'intrinsic_write_file',
                'get_time_ms': 'intrinsic_get_time_ms',
                'get_time_us': 'intrinsic_get_time_us',
                'arena_create': 'intrinsic_arena_create',
                'arena_alloc': 'intrinsic_arena_alloc',
                'arena_reset': 'intrinsic_arena_reset',
                'arena_free': 'intrinsic_arena_free',
                'arena_used': 'intrinsic_arena_used',
                'sb_new': 'intrinsic_sb_new',
                'sb_new_cap': 'intrinsic_sb_new_cap',
                'sb_append': 'intrinsic_sb_append',
                'sb_append_char': 'intrinsic_sb_append_char',
                'sb_append_i64': 'intrinsic_sb_append_i64',
                'sb_build': 'intrinsic_sb_to_string',
                'sb_to_string': 'intrinsic_sb_to_string',
                'sb_clear': 'intrinsic_sb_clear',
                'sb_free': 'intrinsic_sb_free',
                'sb_len': 'intrinsic_sb_len',
                'panic': 'intrinsic_panic',
                # Phase 7: Actor Runtime
                'get_num_cpus': 'intrinsic_get_num_cpus',
                'thread_spawn': 'intrinsic_thread_spawn',
                'thread_join': 'intrinsic_thread_join',
                'thread_id': 'intrinsic_thread_id',
                'sleep_ms': 'intrinsic_sleep_ms',
                'thread_yield': 'intrinsic_thread_yield',
                'mutex_new': 'intrinsic_mutex_new',
                'mutex_lock': 'intrinsic_mutex_lock',
                'mutex_unlock': 'intrinsic_mutex_unlock',
                'mutex_free': 'intrinsic_mutex_free',
                'condvar_new': 'intrinsic_condvar_new',
                'condvar_wait': 'intrinsic_condvar_wait',
                'condvar_signal': 'intrinsic_condvar_signal',
                'condvar_broadcast': 'intrinsic_condvar_broadcast',
                'condvar_free': 'intrinsic_condvar_free',
                'atomic_load': 'intrinsic_atomic_load',
                'atomic_store': 'intrinsic_atomic_store',
                'atomic_add': 'intrinsic_atomic_add',
                'atomic_sub': 'intrinsic_atomic_sub',
                'atomic_cas': 'intrinsic_atomic_cas',
                'atomic_load_ptr': 'intrinsic_atomic_load_ptr',
                'atomic_store_ptr': 'intrinsic_atomic_store_ptr',
                'atomic_cas_ptr': 'intrinsic_atomic_cas_ptr',
                # Phase 23.3: Lock-free mailbox - no intrinsic mapping, use direct calls
                'actor_spawn': 'intrinsic_actor_spawn',
                'actor_send': 'intrinsic_actor_send',
                'actor_state': 'intrinsic_actor_state',
                'actor_set_state': 'intrinsic_actor_set_state',
                'actor_mailbox': 'intrinsic_actor_mailbox',
                'actor_id': 'intrinsic_actor_id',
                'actor_ask': 'intrinsic_actor_ask',
                # Phase 4.7: Actor Checkpointing
                'actor_checkpoint_save': 'actor_checkpoint_save',
                'actor_checkpoint_load': 'actor_checkpoint_load',
                'actor_checkpoint_get_id': 'actor_checkpoint_get_id',
                'actor_checkpoint_exists': 'actor_checkpoint_exists',
                'actor_checkpoint_delete': 'actor_checkpoint_delete',
                'actor_spawn_from_checkpoint': 'actor_spawn_from_checkpoint',
                # Phase 10: Content-Addressed Code
                'code_store_new': 'code_store_new',
                'code_store_put': 'code_store_put',
                'code_store_get': 'code_store_get',
                'code_store_put_ast': 'code_store_put_ast',
                'code_store_get_ast': 'code_store_get_ast',
                'code_store_close': 'code_store_close',
                # Phase 8: Async Runtime
                'io_driver_new': 'intrinsic_io_driver_new',
                'io_driver_init': 'intrinsic_io_driver_init',
                'io_driver_register_read': 'intrinsic_io_driver_register_read',
                'io_driver_register_write': 'intrinsic_io_driver_register_write',
                'io_driver_unregister': 'intrinsic_io_driver_unregister',
                'io_driver_poll': 'intrinsic_io_driver_poll',
                'io_driver_free': 'intrinsic_io_driver_free',
                'set_nonblocking': 'intrinsic_set_nonblocking',
                'timer_wheel_new': 'intrinsic_timer_wheel_new',
                'timer_wheel_init': 'intrinsic_timer_wheel_init',
                'timer_register': 'intrinsic_timer_register',
                'timer_check': 'intrinsic_timer_check',
                'timer_next_deadline': 'intrinsic_timer_next_deadline',
                'timer_wheel_free': 'intrinsic_timer_wheel_free',
                'executor_new': 'intrinsic_executor_new',
                'executor_init': 'intrinsic_executor_init',
                'executor_spawn': 'intrinsic_executor_spawn',
                'executor_wake': 'intrinsic_executor_wake',
                'executor_run': 'intrinsic_executor_run',
                'executor_stop': 'intrinsic_executor_stop',
                'executor_free': 'intrinsic_executor_free',
                'now_ms': 'intrinsic_now_ms',
                'call1': 'call1',  # Call function pointer with 1 arg
                'call2': 'call2',  # Call function pointer with 2 args
                # Phase 9: Networking & Platform
                'socket_create': 'intrinsic_socket_create',
                'socket_set_nonblocking': 'intrinsic_socket_set_nonblocking',
                'socket_set_reuseaddr': 'intrinsic_socket_set_reuseaddr',
                'socket_bind': 'intrinsic_socket_bind',
                'socket_listen': 'intrinsic_socket_listen',
                'socket_accept': 'intrinsic_socket_accept',
                'socket_connect': 'intrinsic_socket_connect',
                'socket_read': 'intrinsic_socket_read',
                'socket_write': 'intrinsic_socket_write',
                'socket_close': 'intrinsic_socket_close',
                'socket_get_error': 'intrinsic_socket_get_error',
                'socket_sendto': 'intrinsic_socket_sendto',
                'socket_recvfrom': 'intrinsic_socket_recvfrom',
                'dns_resolve': 'intrinsic_dns_resolve',
                'ip_to_string': 'intrinsic_ip_to_string',
                'string_to_ip': 'intrinsic_string_to_ip',
                'get_errno': 'intrinsic_get_errno',
                'random_seed': 'intrinsic_random_seed',
                'random_i64': 'intrinsic_random_i64',
                'getcwd': 'intrinsic_getcwd',
                'getenv': 'intrinsic_getenv',
                'setenv': 'intrinsic_setenv',
                # Phase 10: Distribution & Persistence
                'ser_writer_new': 'intrinsic_ser_writer_new',
                'ser_write_u8': 'intrinsic_ser_write_u8',
                'ser_write_u16': 'intrinsic_ser_write_u16',
                'ser_write_u32': 'intrinsic_ser_write_u32',
                'ser_write_i64': 'intrinsic_ser_write_i64',
                'ser_write_bytes': 'intrinsic_ser_write_bytes',
                'ser_write_string': 'intrinsic_ser_write_string',
                'ser_writer_bytes': 'intrinsic_ser_writer_bytes',
                'ser_writer_len': 'intrinsic_ser_writer_len',
                'ser_writer_free': 'intrinsic_ser_writer_free',
                'ser_reader_new': 'intrinsic_ser_reader_new',
                'ser_read_u8': 'intrinsic_ser_read_u8',
                'ser_read_u16': 'intrinsic_ser_read_u16',
                'ser_read_u32': 'intrinsic_ser_read_u32',
                'ser_read_i64': 'intrinsic_ser_read_i64',
                'ser_read_bytes': 'intrinsic_ser_read_bytes',
                'ser_read_string': 'intrinsic_ser_read_string',
                'ser_reader_remaining': 'intrinsic_ser_reader_remaining',
                'ser_reader_free': 'intrinsic_ser_reader_free',
                'sha256': 'intrinsic_sha256',
                'checkpoint_save': 'intrinsic_checkpoint_save',
                'checkpoint_load': 'intrinsic_checkpoint_load',
                'checkpoint_delete': 'intrinsic_checkpoint_delete',
                'generate_node_id': 'intrinsic_generate_node_id',
                'rpc_send': 'intrinsic_rpc_send',
                'rpc_recv': 'intrinsic_rpc_recv',
                # Phase 11: Complete Toolchain
                'process_run': 'intrinsic_process_run',
                'process_output': 'intrinsic_process_output',
                'file_exists': 'intrinsic_file_exists',
                'is_directory': 'intrinsic_is_directory',
                'is_file': 'intrinsic_is_file',
                'mkdir_p': 'intrinsic_mkdir_p',
                'remove_path': 'intrinsic_remove_path',
                'file_size': 'intrinsic_file_size',
                'file_mtime': 'intrinsic_file_mtime',
                'temp_file': 'intrinsic_temp_file',
                'temp_dir': 'intrinsic_temp_dir',
                'path_join': 'intrinsic_path_join',
                'path_dirname': 'intrinsic_path_dirname',
                'path_basename': 'intrinsic_path_basename',
                'path_extension': 'intrinsic_path_extension',
                'assert_fail': 'intrinsic_assert_fail',
                'assert_eq_i64': 'intrinsic_assert_eq_i64',
                'assert_eq_str': 'intrinsic_assert_eq_str',
                'args_count': 'intrinsic_args_count',
                'args_get': 'intrinsic_args_get',
                # Phase 12: Memory Substrate
                'remember': 'intrinsic_remember',
                'recall': 'intrinsic_recall',
                'recall_one': 'intrinsic_recall_one',
                'forget': 'intrinsic_forget',
                'forget_all': 'intrinsic_forget_all',
                'memory_count': 'intrinsic_memory_count',
                'memory_prune': 'intrinsic_memory_prune',
                'memory_decay': 'intrinsic_memory_decay',
                'memory_importance': 'intrinsic_memory_importance',
                'memory_set_importance': 'intrinsic_memory_set_importance',
                # Phase 13: Belief System
                'believe': 'intrinsic_believe',
                'infer_belief': 'intrinsic_infer_belief',
                'query_beliefs': 'intrinsic_query_beliefs',
                'get_belief': 'intrinsic_get_belief',
                'belief_confidence': 'intrinsic_belief_confidence',
                'belief_truth': 'intrinsic_belief_truth',
                'update_belief': 'intrinsic_update_belief',
                'revoke_belief': 'intrinsic_revoke_belief',
                'belief_count': 'intrinsic_belief_count',
                'decay_beliefs': 'intrinsic_decay_beliefs',
                # Phase 14: BDI Agent Architecture
                'create_goal': 'intrinsic_create_goal',
                'get_goal': 'intrinsic_get_goal',
                'goal_priority': 'intrinsic_goal_priority',
                'goal_status': 'intrinsic_goal_status',
                'set_goal_status': 'intrinsic_set_goal_status',
                'active_goals': 'intrinsic_active_goals',
                'abandon_goal': 'intrinsic_abandon_goal',
                'create_intention': 'intrinsic_create_intention',
                'intention_step': 'intrinsic_intention_step',
                'advance_intention': 'intrinsic_advance_intention',
                'complete_intention': 'intrinsic_complete_intention',
                'fail_intention': 'intrinsic_fail_intention',
                'pending_intentions': 'intrinsic_pending_intentions',
                'deliberate': 'intrinsic_deliberate',
                # Phase 15: Mnemonic Specialists
                'specialist_create': 'intrinsic_specialist_create',
                'specialist_name': 'intrinsic_specialist_name',
                'specialist_model': 'intrinsic_specialist_model',
                'specialist_domain': 'intrinsic_specialist_domain',
                'specialist_configure': 'intrinsic_specialist_configure',
                'specialist_limits': 'intrinsic_specialist_limits',
                # Phase 16: Evolution Engine
                'trait_create': 'intrinsic_trait_create',
                'trait_value': 'intrinsic_trait_value',
                'trait_mutate': 'intrinsic_trait_mutate',
                'trait_crossover': 'intrinsic_trait_crossover',
                'generation_current': 'intrinsic_generation_current',
                'generation_advance': 'intrinsic_generation_advance',
                'fitness_set': 'intrinsic_fitness_set',
                'fitness_get': 'intrinsic_fitness_get',
                'select_best': 'intrinsic_select_best',
                # Phase 17: Collective Intelligence
                'swarm_broadcast': 'intrinsic_swarm_broadcast',
                'swarm_receive': 'intrinsic_swarm_receive',
                'swarm_messages': 'intrinsic_swarm_messages',
                'vote_cast': 'intrinsic_vote_cast',
                'vote_tally': 'intrinsic_vote_tally',
                'consensus_check': 'intrinsic_consensus_check',
                'swarm_clear': 'intrinsic_swarm_clear',
                # Phase 20: Toolchain Support
                'read_line': 'intrinsic_read_line',
                'print': 'intrinsic_print',
                'is_tty': 'intrinsic_is_tty',
                'stdin_has_data': 'intrinsic_stdin_has_data',
                'string_hash': 'intrinsic_string_hash',
                'string_find': 'intrinsic_string_find',
                'string_trim': 'intrinsic_string_trim',
                'string_split': 'intrinsic_string_split',
                'string_starts_with': 'intrinsic_string_starts_with',
                'string_ends_with': 'intrinsic_string_ends_with',
                'string_contains': 'intrinsic_string_contains',
                'string_replace': 'intrinsic_string_replace',
                'copy_file': 'intrinsic_copy_file',
                'get_home_dir': 'intrinsic_get_home_dir',
            }

            is_intrinsic = orig_func_name in intrinsic_map
            if is_intrinsic:
                func_name = intrinsic_map[orig_func_name]
            # else func_name is already set (could be mangled for generics)

            # Define argument and return types for intrinsics
            # Format: (arg_types, ret_type) where types are 'i64', 'ptr', or 'void'
            intrinsic_types = {
                'malloc': (['i64'], 'ptr'),
                'free': (['ptr'], 'void'),
                'store_ptr': (['ptr', 'i64', 'ptr'], 'ptr'),
                'store_i64': (['ptr', 'i64', 'i64'], 'ptr'),
                'load_ptr': (['ptr', 'i64'], 'ptr'),
                'load_i64': (['ptr', 'i64'], 'i64'),
                'intrinsic_string_len': (['ptr'], 'i64'),
                'intrinsic_string_char_at': (['ptr', 'i64'], 'i64'),
                'intrinsic_string_slice': (['ptr', 'i64', 'i64'], 'ptr'),
                'intrinsic_string_eq': (['ptr', 'ptr'], 'i1'),
                'intrinsic_string_new': (['ptr'], 'ptr'),
                'intrinsic_string_from_char': (['i64'], 'ptr'),
                'intrinsic_string_concat': (['ptr', 'ptr'], 'ptr'),
                'intrinsic_vec_new': ([], 'ptr'),
                'intrinsic_vec_push': (['ptr', 'ptr'], 'void'),
                'intrinsic_vec_get': (['ptr', 'i64'], 'ptr'),
                'intrinsic_vec_len': (['ptr'], 'i64'),
                'intrinsic_println': (['ptr'], 'void'),
                'intrinsic_print': (['ptr'], 'void'),
                'intrinsic_int_to_string': (['i64'], 'ptr'),
                'intrinsic_string_to_int': (['ptr'], 'i64'),
                'intrinsic_get_args': ([], 'ptr'),
                'intrinsic_read_file': (['ptr'], 'ptr'),
                'intrinsic_write_file': (['ptr', 'ptr'], 'void'),
                'intrinsic_get_time_ms': ([], 'i64'),
                'intrinsic_get_time_us': ([], 'i64'),
                'intrinsic_arena_create': (['i64'], 'ptr'),
                'intrinsic_arena_alloc': (['ptr', 'i64'], 'ptr'),
                'intrinsic_arena_reset': (['ptr'], 'void'),
                'intrinsic_arena_free': (['ptr'], 'void'),
                'intrinsic_arena_used': (['ptr'], 'i64'),
                'intrinsic_sb_new': ([], 'ptr'),
                'intrinsic_sb_new_cap': (['i64'], 'ptr'),
                'intrinsic_sb_append': (['ptr', 'ptr'], 'void'),
                'intrinsic_sb_append_char': (['ptr', 'i64'], 'void'),
                'intrinsic_sb_append_i64': (['ptr', 'i64'], 'void'),
                'intrinsic_sb_to_string': (['ptr'], 'ptr'),
                'intrinsic_sb_clear': (['ptr'], 'void'),
                'intrinsic_sb_free': (['ptr'], 'void'),
                'intrinsic_sb_len': (['ptr'], 'i64'),
                'intrinsic_panic': (['ptr'], 'void'),
                # Phase 7: Actor Runtime
                'intrinsic_get_num_cpus': ([], 'i64'),
                'intrinsic_thread_spawn': (['ptr', 'ptr'], 'ptr'),
                'intrinsic_thread_join': (['ptr'], 'void'),
                'intrinsic_thread_id': (['ptr'], 'i64'),
                'intrinsic_sleep_ms': (['i64'], 'void'),
                'intrinsic_thread_yield': ([], 'void'),
                'intrinsic_mutex_new': ([], 'ptr'),
                'intrinsic_mutex_lock': (['ptr'], 'void'),
                'intrinsic_mutex_unlock': (['ptr'], 'void'),
                'intrinsic_mutex_free': (['ptr'], 'void'),
                'intrinsic_condvar_new': ([], 'ptr'),
                'intrinsic_condvar_wait': (['ptr', 'ptr'], 'void'),
                'intrinsic_condvar_signal': (['ptr'], 'void'),
                'intrinsic_condvar_broadcast': (['ptr'], 'void'),
                'intrinsic_condvar_free': (['ptr'], 'void'),
                'intrinsic_atomic_load': (['ptr'], 'i64'),
                'intrinsic_atomic_store': (['ptr', 'i64'], 'void'),
                'intrinsic_atomic_add': (['ptr', 'i64'], 'i64'),
                'intrinsic_atomic_sub': (['ptr', 'i64'], 'i64'),
                'intrinsic_atomic_cas': (['ptr', 'i64', 'i64'], 'i1'),
                'intrinsic_atomic_load_ptr': (['ptr'], 'ptr'),
                'intrinsic_atomic_store_ptr': (['ptr', 'ptr'], 'void'),
                'intrinsic_atomic_cas_ptr': (['ptr', 'ptr', 'ptr'], 'i1'),
                'intrinsic_mailbox_new': ([], 'ptr'),
                'intrinsic_mailbox_send': (['ptr', 'ptr'], 'void'),
                'intrinsic_mailbox_recv': (['ptr'], 'ptr'),
                'intrinsic_mailbox_empty': (['ptr'], 'i1'),
                'intrinsic_mailbox_len': (['ptr'], 'i64'),
                'intrinsic_mailbox_free': (['ptr'], 'void'),
                'intrinsic_actor_spawn': (['ptr', 'ptr'], 'ptr'),
                'intrinsic_actor_send': (['ptr', 'ptr'], 'void'),
                'intrinsic_actor_state': (['ptr'], 'ptr'),
                'intrinsic_actor_set_state': (['ptr', 'ptr'], 'void'),
                'intrinsic_actor_mailbox': (['ptr'], 'ptr'),
                'intrinsic_actor_id': (['ptr'], 'i64'),
                'intrinsic_actor_ask': (['ptr', 'ptr'], 'ptr'),
                # Phase 4.7: Actor Checkpointing
                'actor_checkpoint_save': (['i64', 'ptr', 'i64'], 'i64'),
                'actor_checkpoint_load': (['ptr', 'i64'], 'i64'),
                'actor_checkpoint_get_id': (['ptr'], 'i64'),
                'actor_checkpoint_exists': (['ptr'], 'i64'),
                'actor_checkpoint_delete': (['ptr'], 'i64'),
                'actor_spawn_from_checkpoint': (['ptr', 'ptr', 'i64'], 'i64'),
                # Phase 10: Content-Addressed Code
                'code_store_new': ([], 'i64'),
                'code_store_put': (['i64', 'i64'], 'i64'),
                'code_store_get': (['i64', 'i64'], 'i64'),
                'code_store_put_ast': (['i64', 'i64', 'i64'], 'i64'),
                'code_store_get_ast': (['i64', 'i64'], 'i64'),
                'code_store_close': (['i64'], 'void'),
                # Phase 8: Async Runtime
                'intrinsic_io_driver_new': ([], 'ptr'),
                'intrinsic_io_driver_init': ([], 'void'),
                'intrinsic_io_driver_register_read': (['ptr', 'i64', 'ptr'], 'void'),
                'intrinsic_io_driver_register_write': (['ptr', 'i64', 'ptr'], 'void'),
                'intrinsic_io_driver_unregister': (['ptr', 'i64'], 'void'),
                'intrinsic_io_driver_poll': (['ptr', 'i64'], 'i64'),
                'intrinsic_io_driver_free': (['ptr'], 'void'),
                'intrinsic_set_nonblocking': (['i64'], 'i1'),
                'intrinsic_timer_wheel_new': ([], 'ptr'),
                'intrinsic_timer_wheel_init': ([], 'void'),
                'intrinsic_timer_register': (['ptr', 'i64', 'ptr'], 'void'),
                'intrinsic_timer_check': (['ptr'], 'i64'),
                'intrinsic_timer_next_deadline': (['ptr'], 'i64'),
                'intrinsic_timer_wheel_free': (['ptr'], 'void'),
                'intrinsic_executor_new': ([], 'ptr'),
                'intrinsic_executor_init': ([], 'void'),
                'intrinsic_executor_spawn': (['ptr', 'ptr', 'ptr'], 'i64'),
                'intrinsic_executor_wake': (['ptr', 'i64'], 'void'),
                'intrinsic_executor_run': (['ptr'], 'void'),
                'intrinsic_executor_stop': (['ptr'], 'void'),
                'intrinsic_executor_free': (['ptr'], 'void'),
                'intrinsic_now_ms': ([], 'i64'),
                # Phase 9: Networking & Platform
                'intrinsic_socket_create': (['i64', 'i64'], 'i64'),
                'intrinsic_socket_set_nonblocking': (['i64'], 'void'),
                'intrinsic_socket_set_reuseaddr': (['i64'], 'void'),
                'intrinsic_socket_bind': (['i64', 'i64', 'i64'], 'i64'),
                'intrinsic_socket_listen': (['i64', 'i64'], 'i64'),
                'intrinsic_socket_accept': (['i64', 'ptr', 'ptr'], 'i64'),
                'intrinsic_socket_connect': (['i64', 'i64', 'i64'], 'i64'),
                'intrinsic_socket_read': (['i64', 'ptr', 'i64'], 'i64'),
                'intrinsic_socket_write': (['i64', 'ptr', 'i64'], 'i64'),
                'intrinsic_socket_close': (['i64'], 'void'),
                'intrinsic_socket_get_error': (['i64'], 'i64'),
                'intrinsic_socket_sendto': (['i64', 'ptr', 'i64', 'i64', 'i64'], 'i64'),
                'intrinsic_socket_recvfrom': (['i64', 'ptr', 'i64', 'ptr', 'ptr'], 'i64'),
                'intrinsic_dns_resolve': (['ptr', 'ptr'], 'i64'),
                'intrinsic_ip_to_string': (['i64'], 'ptr'),
                'intrinsic_getenv': (['ptr'], 'ptr'),
                'intrinsic_string_to_ip': (['ptr'], 'i64'),
                'intrinsic_get_errno': ([], 'i64'),
                'intrinsic_random_seed': (['i64'], 'void'),
                'intrinsic_random_i64': ([], 'i64'),
                'intrinsic_getcwd': ([], 'ptr'),
                'intrinsic_setenv': (['ptr', 'ptr'], 'void'),
                # Phase 10: Distribution & Persistence
                'intrinsic_ser_writer_new': ([], 'ptr'),
                'intrinsic_ser_write_u8': (['ptr', 'i64'], 'void'),
                'intrinsic_ser_write_u16': (['ptr', 'i64'], 'void'),
                'intrinsic_ser_write_u32': (['ptr', 'i64'], 'void'),
                'intrinsic_ser_write_i64': (['ptr', 'i64'], 'void'),
                'intrinsic_ser_write_bytes': (['ptr', 'ptr', 'i64'], 'void'),
                'intrinsic_ser_write_string': (['ptr', 'ptr'], 'void'),
                'intrinsic_ser_writer_bytes': (['ptr'], 'ptr'),
                'intrinsic_ser_writer_len': (['ptr'], 'i64'),
                'intrinsic_ser_writer_free': (['ptr'], 'void'),
                'intrinsic_ser_reader_new': (['ptr', 'i64'], 'ptr'),
                'intrinsic_ser_read_u8': (['ptr'], 'i64'),
                'intrinsic_ser_read_u16': (['ptr'], 'i64'),
                'intrinsic_ser_read_u32': (['ptr'], 'i64'),
                'intrinsic_ser_read_i64': (['ptr'], 'i64'),
                'intrinsic_ser_read_bytes': (['ptr'], 'ptr'),
                'intrinsic_ser_read_string': (['ptr'], 'ptr'),
                'intrinsic_ser_reader_remaining': (['ptr'], 'i64'),
                'intrinsic_ser_reader_free': (['ptr'], 'void'),
                'intrinsic_sha256': (['ptr', 'i64'], 'ptr'),
                'intrinsic_checkpoint_save': (['ptr', 'i64', 'ptr', 'i64'], 'i64'),
                'intrinsic_checkpoint_load': (['ptr', 'ptr'], 'ptr'),
                'intrinsic_checkpoint_delete': (['ptr'], 'i64'),
                'intrinsic_generate_node_id': ([], 'i64'),
                'intrinsic_rpc_send': (['i64', 'ptr', 'i64'], 'i64'),
                'intrinsic_rpc_recv': (['i64'], 'ptr'),
                # Phase 11: Complete Toolchain
                'intrinsic_process_run': (['ptr'], 'i64'),
                'intrinsic_process_output': (['ptr'], 'ptr'),
                'intrinsic_file_exists': (['ptr'], 'i64'),
                'intrinsic_is_directory': (['ptr'], 'i64'),
                'intrinsic_is_file': (['ptr'], 'i64'),
                'intrinsic_mkdir_p': (['ptr'], 'i64'),
                'intrinsic_remove_path': (['ptr'], 'i64'),
                'intrinsic_file_size': (['ptr'], 'i64'),
                'intrinsic_file_mtime': (['ptr'], 'i64'),
                'intrinsic_temp_file': (['ptr'], 'ptr'),
                'intrinsic_temp_dir': ([], 'ptr'),
                'intrinsic_path_join': (['ptr', 'ptr'], 'ptr'),
                'intrinsic_path_dirname': (['ptr'], 'ptr'),
                'intrinsic_path_basename': (['ptr'], 'ptr'),
                'intrinsic_path_extension': (['ptr'], 'ptr'),
                'intrinsic_assert_fail': (['ptr', 'ptr', 'i64'], 'void'),
                'intrinsic_assert_eq_i64': (['i64', 'i64', 'ptr', 'i64'], 'void'),
                'intrinsic_assert_eq_str': (['ptr', 'ptr', 'ptr', 'i64'], 'void'),
                'intrinsic_args_count': ([], 'i64'),
                'intrinsic_args_get': (['i64'], 'ptr'),
                # Phase 12: Memory Substrate
                # Note: importance passed as i64 (0-100 scale) since bootstrap lacks f64
                'intrinsic_remember': (['ptr', 'i64', 'i64'], 'i64'),
                'intrinsic_recall': (['ptr', 'i64'], 'ptr'),
                'intrinsic_recall_one': (['i64'], 'ptr'),
                'intrinsic_forget': (['i64'], 'i64'),
                'intrinsic_forget_all': ([], 'void'),
                'intrinsic_memory_count': ([], 'i64'),
                'intrinsic_memory_prune': ([], 'i64'),
                'intrinsic_memory_decay': (['i64'], 'void'),
                'intrinsic_memory_importance': (['i64'], 'i64'),
                'intrinsic_memory_set_importance': (['i64', 'i64'], 'void'),
                # Phase 13: Belief System
                'intrinsic_believe': (['ptr', 'i64', 'i64'], 'i64'),
                'intrinsic_infer_belief': (['ptr', 'ptr', 'i64'], 'i64'),
                'intrinsic_query_beliefs': (['i64', 'i64'], 'ptr'),
                'intrinsic_get_belief': (['i64'], 'ptr'),
                'intrinsic_belief_confidence': (['i64'], 'i64'),
                'intrinsic_belief_truth': (['i64'], 'i64'),
                'intrinsic_update_belief': (['i64', 'i64'], 'void'),
                'intrinsic_revoke_belief': (['i64'], 'i64'),
                'intrinsic_belief_count': ([], 'i64'),
                'intrinsic_decay_beliefs': ([], 'void'),
                # Phase 14: BDI Agent Architecture
                'intrinsic_create_goal': (['ptr', 'ptr', 'i64'], 'i64'),
                'intrinsic_get_goal': (['i64'], 'ptr'),
                'intrinsic_goal_priority': (['i64'], 'i64'),
                'intrinsic_goal_status': (['i64'], 'i64'),
                'intrinsic_set_goal_status': (['i64', 'i64'], 'void'),
                'intrinsic_active_goals': ([], 'ptr'),
                'intrinsic_abandon_goal': (['i64'], 'void'),
                'intrinsic_create_intention': (['i64', 'ptr', 'i64'], 'i64'),
                'intrinsic_intention_step': (['i64'], 'i64'),
                'intrinsic_advance_intention': (['i64'], 'i64'),
                'intrinsic_complete_intention': (['i64'], 'void'),
                'intrinsic_fail_intention': (['i64'], 'void'),
                'intrinsic_pending_intentions': ([], 'ptr'),
                'intrinsic_deliberate': ([], 'i64'),
                # Phase 15: Mnemonic Specialists
                'intrinsic_specialist_create': (['ptr', 'ptr', 'ptr'], 'i64'),
                'intrinsic_specialist_name': (['i64'], 'ptr'),
                'intrinsic_specialist_model': (['i64'], 'ptr'),
                'intrinsic_specialist_domain': (['i64'], 'ptr'),
                'intrinsic_specialist_configure': (['i64', 'i64', 'i64', 'i64'], 'void'),
                'intrinsic_specialist_limits': (['i64'], 'ptr'),
                # Phase 16: Evolution Engine
                'intrinsic_trait_create': (['ptr', 'i64'], 'i64'),
                'intrinsic_trait_value': (['i64'], 'i64'),
                'intrinsic_trait_mutate': (['i64', 'i64'], 'i64'),
                'intrinsic_trait_crossover': (['i64', 'i64'], 'i64'),
                'intrinsic_generation_current': ([], 'i64'),
                'intrinsic_generation_advance': ([], 'i64'),
                'intrinsic_fitness_set': (['i64', 'i64'], 'void'),
                'intrinsic_fitness_get': (['i64'], 'i64'),
                'intrinsic_select_best': (['i64'], 'ptr'),
                # Phase 17: Collective Intelligence
                'intrinsic_swarm_broadcast': (['i64', 'i64', 'ptr'], 'i64'),
                'intrinsic_swarm_receive': (['i64', 'i64'], 'ptr'),
                'intrinsic_swarm_messages': (['i64'], 'ptr'),
                'intrinsic_vote_cast': (['i64', 'i64'], 'void'),
                'intrinsic_vote_tally': (['i64'], 'ptr'),
                'intrinsic_consensus_check': (['i64', 'i64'], 'i64'),
                'intrinsic_swarm_clear': (['i64'], 'void'),
                # Phase 20: Toolchain Support
                'intrinsic_read_line': ([], 'ptr'),
                'intrinsic_print': (['ptr'], 'void'),
                'intrinsic_is_tty': ([], 'i1'),
                'intrinsic_stdin_has_data': ([], 'i1'),
                'intrinsic_string_hash': (['ptr'], 'i64'),
                'intrinsic_string_find': (['ptr', 'ptr', 'i64'], 'i64'),
                'intrinsic_string_trim': (['ptr'], 'ptr'),
                'intrinsic_string_split': (['ptr', 'ptr'], 'ptr'),
                'intrinsic_string_starts_with': (['ptr', 'ptr'], 'i1'),
                'intrinsic_string_ends_with': (['ptr', 'ptr'], 'i1'),
                'intrinsic_string_contains': (['ptr', 'ptr'], 'i1'),
                'intrinsic_string_replace': (['ptr', 'ptr', 'ptr'], 'ptr'),
                'intrinsic_copy_file': (['ptr', 'ptr'], 'i64'),
                'intrinsic_get_home_dir': ([], 'ptr'),
                # Phase 24.5: f64 Math
                'f64_add': (['double', 'double'], 'double'),
                'f64_sub': (['double', 'double'], 'double'),
                'f64_mul': (['double', 'double'], 'double'),
                'f64_div': (['double', 'double'], 'double'),
                'f64_neg': (['double'], 'double'),
                'f64_abs': (['double'], 'double'),
                'f64_eq': (['double', 'double'], 'i64'),
                'f64_ne': (['double', 'double'], 'i64'),
                'f64_lt': (['double', 'double'], 'i64'),
                'f64_le': (['double', 'double'], 'i64'),
                'f64_gt': (['double', 'double'], 'i64'),
                'f64_ge': (['double', 'double'], 'i64'),
                'f64_sqrt': (['double'], 'double'),
                'f64_pow': (['double', 'double'], 'double'),
                'f64_sin': (['double'], 'double'),
                'f64_cos': (['double'], 'double'),
                'f64_tan': (['double'], 'double'),
                'f64_asin': (['double'], 'double'),
                'f64_acos': (['double'], 'double'),
                'f64_atan': (['double'], 'double'),
                'f64_atan2': (['double', 'double'], 'double'),
                'f64_exp': (['double'], 'double'),
                'f64_log': (['double'], 'double'),
                'f64_log10': (['double'], 'double'),
                'f64_log2': (['double'], 'double'),
                'f64_floor': (['double'], 'double'),
                'f64_ceil': (['double'], 'double'),
                'f64_round': (['double'], 'double'),
                'f64_trunc': (['double'], 'double'),
                'f64_min': (['double', 'double'], 'double'),
                'f64_max': (['double', 'double'], 'double'),
                'f64_from_i64': (['i64'], 'double'),
                'f64_to_i64': (['double'], 'i64'),
                'f64_to_string': (['double'], 'ptr'),
                'f64_from_string': (['ptr'], 'double'),
                # Phase 26 functions with double params
                'embedding_get': (['i64', 'i64'], 'double'),
                'embedding_cosine_similarity': (['i64', 'i64'], 'double'),
                'memdb_store': (['i64', 'i64', 'i64', 'double'], 'i64'),
                'memdb_get_importance': (['i64', 'i64'], 'double'),
                'memdb_set_importance': (['i64', 'i64', 'double'], 'i64'),
                'cluster_manager_new': (['double'], 'i64'),
                'prune_set_min_importance': (['i64', 'double'], 'i64'),
                'importance_calculate': (['double', 'double', 'double', 'double'], 'double'),
                'importance_decay': (['double', 'double', 'double'], 'double'),
                'importance_boost': (['double', 'double'], 'double'),
                # Phase 27 functions with double params
                'belief_store_add': (['i64', 'i64', 'double', 'i64'], 'i64'),
                'belief_store_confidence': (['i64', 'i64'], 'double'),
                'belief_query_by_confidence': (['i64', 'double'], 'i64'),
                # Phase 28 functions with double params
                'goal_set_priority': (['i64', 'double'], 'i64'),
                'goal_get_priority': (['i64'], 'double'),
                'bdi_add_belief': (['i64', 'i64', 'double'], 'i64'),
                # Phase 30 functions with double params
                'individual_set_gene': (['i64', 'i64', 'double'], 'i64'),
                'individual_get_gene': (['i64', 'i64'], 'double'),
                'individual_fitness': (['i64'], 'double'),
                'individual_set_fitness': (['i64', 'double'], 'i64'),
                'population_avg_fitness': (['i64'], 'double'),
                'crossover_uniform': (['i64', 'i64', 'double'], 'i64'),
                'mutation_gaussian': (['i64', 'double'], 'i64'),
                'mutation_uniform': (['i64', 'double', 'double'], 'i64'),
                'mutation_bit_flip': (['i64', 'double'], 'i64'),
                # Phase 31 functions with double params
                'pheromone_deposit': (['i64', 'i64', 'i64', 'double'], 'i64'),
                'pheromone_read': (['i64', 'i64', 'i64'], 'double'),
                'pheromone_evaporate': (['i64', 'double'], 'i64'),
                'swarm_set_position': (['i64', 'i64', 'i64', 'double'], 'i64'),
                'swarm_get_position': (['i64', 'i64', 'i64'], 'double'),
                'swarm_set_velocity': (['i64', 'i64', 'i64', 'double'], 'i64'),
                'swarm_best_fitness': (['i64'], 'double'),
            }

            if func_name in intrinsic_types:
                arg_types, ret_type = intrinsic_types[func_name]

                # Convert arguments to expected types
                converted_args = []
                for i, (arg, expected_type) in enumerate(zip(args, arg_types)):
                    if expected_type == 'ptr':
                        # Convert i64 to ptr
                        conv_temp = self.new_temp()
                        self.emit(f'  {conv_temp} = inttoptr i64 {arg} to ptr')
                        converted_args.append(f'ptr {conv_temp}')
                    elif expected_type == 'double':
                        # Arg might be a double constant or need bitcast from i64
                        # Check if arg looks like a double constant (0x... format)
                        if arg.startswith('0x') or arg.startswith('-') or (arg[0].isdigit() and '.' in str(arg)):
                            converted_args.append(f'double {arg}')
                        else:
                            # Bitcast i64 to double
                            conv_temp = self.new_temp()
                            self.emit(f'  {conv_temp} = bitcast i64 {arg} to double')
                            converted_args.append(f'double {conv_temp}')
                    else:
                        converted_args.append(f'{expected_type} {arg}')
                args_str = ', '.join(converted_args)

                if ret_type == 'void':
                    self.emit(f'  call void @{func_name}({args_str})')
                    return '0'
                elif ret_type == 'i64':
                    self.emit(f'  {temp} = call i64 @{func_name}({args_str})')
                    return temp
                elif ret_type == 'i1':
                    self.emit(f'  {temp} = call i1 @{func_name}({args_str})')
                    temp2 = self.new_temp()
                    self.emit(f'  {temp2} = zext i1 {temp} to i64')
                    return temp2
                elif ret_type == 'double':
                    # Call returns double, bitcast to i64 for storage
                    dbl_temp = self.new_temp()
                    self.emit(f'  {dbl_temp} = call double @{func_name}({args_str})')
                    self.emit(f'  {temp} = bitcast double {dbl_temp} to i64')
                    return temp
                else:  # ptr
                    self.emit(f'  {temp} = call ptr @{func_name}({args_str})')
                    temp2 = self.new_temp()
                    self.emit(f'  {temp2} = ptrtoint ptr {temp} to i64')
                    return temp2
            elif func_name == 'call1':
                # Indirect call with 1 arg: call1(fn_ptr, arg1)
                fn_ptr = args[0]
                arg1 = args[1]
                ptr_temp = self.new_temp()
                self.emit(f'  {ptr_temp} = inttoptr i64 {fn_ptr} to ptr')
                self.emit(f'  {temp} = call i64 {ptr_temp}(i64 {arg1})')
                return temp
            elif func_name == 'call2':
                # Indirect call with 2 args: call2(fn_ptr, arg1, arg2)
                fn_ptr = args[0]
                arg1 = args[1]
                arg2 = args[2]
                ptr_temp = self.new_temp()
                self.emit(f'  {ptr_temp} = inttoptr i64 {fn_ptr} to ptr')
                self.emit(f'  {temp} = call i64 {ptr_temp}(i64 {arg1}, i64 {arg2})')
                return temp
            else:
                # Check if this is a closure call (function name is a local variable)
                if func_name in self.locals:
                    # Closure call: closure struct = {fn_ptr, env_ptr}
                    closure_slot = self.locals[func_name]
                    closure_val = self.new_temp()
                    self.emit(f'  {closure_val} = load i64, ptr {closure_slot}')

                    # Extract function pointer (offset 0)
                    closure_ptr = self.new_temp()
                    self.emit(f'  {closure_ptr} = inttoptr i64 {closure_val} to ptr')
                    fn_ptr_temp = self.new_temp()
                    self.emit(f'  {fn_ptr_temp} = load i64, ptr {closure_ptr}')

                    # Extract environment pointer (offset 8)
                    env_gep = self.new_temp()
                    self.emit(f'  {env_gep} = getelementptr i8, ptr {closure_ptr}, i64 8')
                    env_ptr_temp = self.new_temp()
                    self.emit(f'  {env_ptr_temp} = load i64, ptr {env_gep}')

                    # Call closure: fn_ptr(env_ptr, args...)
                    fn_ptr = self.new_temp()
                    self.emit(f'  {fn_ptr} = inttoptr i64 {fn_ptr_temp} to ptr')
                    all_args = [f'i64 {env_ptr_temp}'] + [f'i64 {a}' for a in args]
                    args_str = ', '.join(all_args)
                    self.emit(f'  {temp} = call i64 {fn_ptr}({args_str})')
                    return temp
                else:
                    # User-defined function - assume all i64
                    args_str = ', '.join(f'i64 {a}' for a in args)
                    self.emit(f'  {temp} = call i64 @"{func_name}"({args_str})')
                    return temp

        if expr_type == 'IfExpr':
            cond = self.generate_expr(expr['condition'])
            then_label = self.new_label('then')
            else_label = self.new_label('else')
            end_label = self.new_label('endif')
            then_end_label = then_label + '_end'
            else_end_label = else_label + '_end'

            # Convert i64 to i1 for branch
            cond_i1 = self.new_temp()
            self.emit(f'  {cond_i1} = icmp ne i64 {cond}, 0')
            self.emit(f'  br i1 {cond_i1}, label %{then_label}, label %{else_label}')

            self.emit(f'{then_label}:')
            then_val = self.generate_block(expr['then_block'])
            self.emit(f'  br label %{then_end_label}')

            self.emit(f'{then_end_label}:')
            self.emit(f'  br label %{end_label}')

            self.emit(f'{else_label}:')
            if expr['else_block']:
                else_val = self.generate_block(expr['else_block'])
            else:
                else_val = '0'
            self.emit(f'  br label %{else_end_label}')

            self.emit(f'{else_end_label}:')
            self.emit(f'  br label %{end_label}')

            self.emit(f'{end_label}:')
            result = self.new_temp()
            self.emit(f'  {result} = phi i64 [{then_val}, %{then_end_label}], [{else_val}, %{else_end_label}]')
            return result

        if expr_type == 'IfLetExpr':
            # if let Pattern = scrutinee { then } else { else }
            # Desugars to pattern match on scrutinee
            pattern = expr['pattern']
            scrutinee_val = self.generate_expr(expr['scrutinee'])

            match_label = self.new_label('iflet_match')
            nomatch_label = self.new_label('iflet_nomatch')
            end_label = self.new_label('iflet_end')

            # Generate pattern matching code
            pattern_type = pattern.get('type')

            if pattern_type == 'EnumPattern':
                # Match enum variant: Some(x), Ok(y), etc.
                # For Option<T>/Result<T,E> style enums, tag is at offset 0
                enum_variant = pattern.get('enum_variant', '')
                bindings = pattern.get('bindings', [])

                # Convert scrutinee to ptr
                ptr_temp = self.new_temp()
                self.emit(f'  {ptr_temp} = inttoptr i64 {scrutinee_val} to ptr')

                # Load tag (offset 0)
                tag_temp = self.new_temp()
                self.emit(f'  {tag_temp} = load i64, ptr {ptr_temp}')

                # Determine expected tag value
                # Some = 1, None = 0, Ok = 1, Err = 0
                if enum_variant in ('Some', 'Ok'):
                    expected_tag = 1
                elif enum_variant in ('None', 'Err'):
                    expected_tag = 0
                else:
                    # Look up enum in registry
                    variant_name = enum_variant.split('::')[-1] if '::' in enum_variant else enum_variant
                    enum_name = enum_variant.split('::')[0] if '::' in enum_variant else None
                    if enum_name and enum_name in self.enums:
                        expected_tag = self.enums[enum_name].get(variant_name, 0)
                    else:
                        expected_tag = 0  # Default

                # Compare tag
                cmp_temp = self.new_temp()
                self.emit(f'  {cmp_temp} = icmp eq i64 {tag_temp}, {expected_tag}')
                self.emit(f'  br i1 {cmp_temp}, label %{match_label}, label %{nomatch_label}')

                # Match case - extract bindings
                self.emit(f'{match_label}:')
                for i, binding in enumerate(bindings):
                    if binding.get('type') == 'BindingPattern':
                        binding_name = binding['name']
                        # Load value from offset (1 + i) * 8
                        offset = (1 + i) * 8
                        gep_temp = self.new_temp()
                        self.emit(f'  {gep_temp} = getelementptr i8, ptr {ptr_temp}, i64 {offset}')
                        val_temp = self.new_temp()
                        self.emit(f'  {val_temp} = load i64, ptr {gep_temp}')
                        # Create local for binding
                        local = self.new_local(binding_name)
                        self.emit(f'  {local} = alloca i64')
                        self.emit(f'  store i64 {val_temp}, ptr {local}')
                        self.locals[binding_name] = local

            elif pattern_type == 'BindingPattern':
                # Simple binding - always matches, just binds the value
                binding_name = pattern['name']
                local = self.new_local(binding_name)
                self.emit(f'  {local} = alloca i64')
                self.emit(f'  store i64 {scrutinee_val}, ptr {local}')
                self.locals[binding_name] = local
                # Always matches
                self.emit(f'  br label %{match_label}')
                self.emit(f'{match_label}:')

            elif pattern_type == 'WildcardPattern':
                # Wildcard always matches
                self.emit(f'  br label %{match_label}')
                self.emit(f'{match_label}:')

            else:
                # Default: treat as always matching
                self.emit(f'  br label %{match_label}')
                self.emit(f'{match_label}:')

            # Generate then block
            then_val = self.generate_block(expr['then_block'])
            self.emit(f'  br label %{end_label}')

            # No-match case
            self.emit(f'{nomatch_label}:')
            if expr.get('else_block'):
                else_val = self.generate_block(expr['else_block'])
            else:
                else_val = '0'
            self.emit(f'  br label %{end_label}')

            # End - phi the result
            self.emit(f'{end_label}:')
            result = self.new_temp()
            # Note: simplified phi, real impl needs proper predecessor tracking
            self.emit(f'  {result} = phi i64 [{then_val}, %{match_label}], [{else_val}, %{nomatch_label}]')
            return result

        if expr_type == 'WhileExpr':
            cond_label = self.new_label('while_cond')
            body_label = self.new_label('while_body')
            end_label = self.new_label('while_end')

            self.emit(f'  br label %{cond_label}')

            self.emit(f'{cond_label}:')
            cond = self.generate_expr(expr['condition'])
            cond_i1 = self.new_temp()
            self.emit(f'  {cond_i1} = icmp ne i64 {cond}, 0')
            self.emit(f'  br i1 {cond_i1}, label %{body_label}, label %{end_label}')

            self.emit(f'{body_label}:')
            self.loop_stack.append((cond_label, end_label))
            self.generate_block(expr['body'])
            self.loop_stack.pop()
            self.emit(f'  br label %{cond_label}')

            self.emit(f'{end_label}:')
            return '0'

        if expr_type == 'ForExpr':
            # For loop: for var in start..end { body }
            # Desugar to: let var = start; while var < end { body; var = var + 1; }
            var_name = expr['var_name']
            start_val = self.generate_expr(expr['start'])
            end_val = self.generate_expr(expr['end'])

            # Allocate loop variable
            local = self.new_local(var_name)
            self.emit(f'  {local} = alloca i64')
            self.emit(f'  store i64 {start_val}, ptr {local}')
            self.locals[var_name] = local

            # Labels for the loop
            cond_label = self.new_label('for_cond')
            body_label = self.new_label('for_body')
            inc_label = self.new_label('for_inc')
            end_label = self.new_label('for_end')

            self.emit(f'  br label %{cond_label}')

            # Condition: var < end
            self.emit(f'{cond_label}:')
            cur_temp = self.new_temp()
            self.emit(f'  {cur_temp} = load i64, ptr {local}')
            cmp_temp = self.new_temp()
            self.emit(f'  {cmp_temp} = icmp slt i64 {cur_temp}, {end_val}')
            self.emit(f'  br i1 {cmp_temp}, label %{body_label}, label %{end_label}')

            # Body
            self.emit(f'{body_label}:')
            self.loop_stack.append((inc_label, end_label))
            self.generate_block(expr['body'])
            self.loop_stack.pop()
            self.emit(f'  br label %{inc_label}')

            # Increment: var = var + 1
            self.emit(f'{inc_label}:')
            load_temp = self.new_temp()
            self.emit(f'  {load_temp} = load i64, ptr {local}')
            inc_temp = self.new_temp()
            self.emit(f'  {inc_temp} = add i64 {load_temp}, 1')
            self.emit(f'  store i64 {inc_temp}, ptr {local}')
            self.emit(f'  br label %{cond_label}')

            self.emit(f'{end_label}:')
            return '0'

        if expr_type == 'WhileLetExpr':
            # while let Pattern = scrutinee { body }
            # Loop that continues while pattern matches
            cond_label = self.new_label('while_let_cond')
            body_label = self.new_label('while_let_body')
            end_label = self.new_label('while_let_end')

            self.emit(f'  br label %{cond_label}')
            self.emit(f'{cond_label}:')

            # Evaluate scrutinee
            scrutinee_val = self.generate_expr(expr['scrutinee'])

            # Check pattern and bind if matched
            pattern = expr['pattern']
            if pattern['type'] == 'EnumPattern':
                # Check tag for enum variant (e.g., Some(x))
                # Convert scrutinee to ptr
                ptr_temp = self.new_temp()
                self.emit(f'  {ptr_temp} = inttoptr i64 {scrutinee_val} to ptr')
                # Load tag from offset 0
                tag_temp = self.new_temp()
                self.emit(f'  {tag_temp} = load i64, ptr {ptr_temp}')

                # For Some pattern (tag 1), check != 0
                variant = pattern.get('enum_variant', pattern.get('variant', 'Some'))
                if variant == 'Some':
                    cond_i1 = self.new_temp()
                    self.emit(f'  {cond_i1} = icmp ne i64 {tag_temp}, 0')
                elif variant == 'None':
                    cond_i1 = self.new_temp()
                    self.emit(f'  {cond_i1} = icmp eq i64 {tag_temp}, 0')
                else:
                    # Generic enum variant check
                    expected_tag = 1  # Assume simple tag
                    cond_i1 = self.new_temp()
                    self.emit(f'  {cond_i1} = icmp eq i64 {tag_temp}, {expected_tag}')

                self.emit(f'  br i1 {cond_i1}, label %{body_label}, label %{end_label}')
                self.emit(f'{body_label}:')

                # Bind inner value if pattern has binding
                # Bindings are stored at offset 8, 16, etc. (after the discriminant at offset 0)
                bindings = pattern.get('bindings', [])
                for i, binding in enumerate(bindings):
                    offset = 8 + i * 8  # Payload starts at offset 8
                    if isinstance(binding, str):
                        binding_name = binding
                    elif isinstance(binding, dict) and binding.get('type') == 'BindingPattern':
                        binding_name = binding['name']
                    else:
                        continue
                    # Load value from struct (ptr_temp already points to the enum struct)
                    gep_temp = self.new_temp()
                    self.emit(f'  {gep_temp} = getelementptr i8, ptr {ptr_temp}, i64 {offset}')
                    val_temp = self.new_temp()
                    self.emit(f'  {val_temp} = load i64, ptr {gep_temp}')
                    local = self.new_local(binding_name)
                    self.emit(f'  {local} = alloca i64')
                    self.emit(f'  store i64 {val_temp}, ptr {local}')
                    self.locals[binding_name] = local
            else:
                # Simple pattern - just check truthiness
                cond_i1 = self.new_temp()
                self.emit(f'  {cond_i1} = icmp ne i64 {scrutinee_val}, 0')
                self.emit(f'  br i1 {cond_i1}, label %{body_label}, label %{end_label}')
                self.emit(f'{body_label}:')

            self.loop_stack.append((cond_label, end_label))
            self.generate_block(expr['body'])
            self.loop_stack.pop()
            self.emit(f'  br label %{cond_label}')

            self.emit(f'{end_label}:')
            return '0'

        if expr_type == 'ForInExpr':
            # for var in collection { body }
            # Desugar to: let iter = collection.iter(); while let Some(var) = iter.next() { body }
            var_name = expr['var']
            iterator = expr['iterator']

            # Generate iterator call
            iter_val = self.generate_expr(iterator)

            # Get iterator (if it's a Vec, call iter())
            iter_ptr = self.new_temp()
            self.emit(f'  {iter_ptr} = call i64 @vec_iter(i64 {iter_val})')

            # Loop labels
            cond_label = self.new_label('for_in_cond')
            body_label = self.new_label('for_in_body')
            end_label = self.new_label('for_in_end')

            self.emit(f'  br label %{cond_label}')
            self.emit(f'{cond_label}:')

            # Call iter.next() - returns Option<T>
            next_val = self.new_temp()
            self.emit(f'  {next_val} = call i64 @iter_next(i64 {iter_ptr})')

            # Check if Some (tag != 0)
            tag_temp = self.new_temp()
            self.emit(f'  {tag_temp} = and i64 {next_val}, 255')
            cond_i1 = self.new_temp()
            self.emit(f'  {cond_i1} = icmp ne i64 {tag_temp}, 0')
            self.emit(f'  br i1 {cond_i1}, label %{body_label}, label %{end_label}')

            self.emit(f'{body_label}:')

            # Extract value and bind to var
            val_temp = self.new_temp()
            self.emit(f'  {val_temp} = lshr i64 {next_val}, 8')
            local = self.new_local(var_name)
            self.emit(f'  {local} = alloca i64')
            self.emit(f'  store i64 {val_temp}, ptr {local}')
            self.locals[var_name] = local

            # Execute body
            self.loop_stack.append((cond_label, end_label))
            self.generate_block(expr['body'])
            self.loop_stack.pop()
            self.emit(f'  br label %{cond_label}')

            self.emit(f'{end_label}:')
            return '0'

        if expr_type == 'AsyncBlockExpr':
            # async { body } - creates a Future
            # Generate as a closure that returns a future
            body = expr['body']

            # Create a unique name for the async block's poll function
            async_id = self.label_counter
            self.label_counter += 1
            poll_fn_name = f'async_block_{async_id}_poll'

            # For now, just generate the body inline and wrap in future
            # Full implementation would create state machine
            temp = self.new_temp()
            self.emit(f'  ; async block - future wrapper')
            result = self.generate_block(body)
            # Wrap result in completed future
            self.emit(f'  {temp} = call i64 @future_ready(i64 {result})')
            return temp

        if expr_type == 'MatchExpr':
            # Match expression: match scrutinee { pattern if guard => result, ... }
            # Lowered to structured control flow with proper label blocks
            scrutinee_val = self.generate_expr(expr['scrutinee'])
            arms = expr['arms']
            num_arms = len(arms)

            # Pre-generate all labels
            match_id = self.label_counter
            self.label_counter += 1
            end_label = f'match_end{match_id}'
            no_match_label = f'no_match{match_id}'
            check_labels = [f'check{match_id}_{i}' for i in range(num_arms)]
            arm_labels = [f'arm{match_id}_{i}' for i in range(num_arms)]
            guard_labels = [f'guard{match_id}_{i}' for i in range(num_arms)]
            arm_end_labels = [f'arm{match_id}_{i}_end' for i in range(num_arms)]
            arm_results = []

            # Branch to first check
            self.emit(f'  br label %{check_labels[0]}')

            for i, arm in enumerate(arms):
                pattern = arm['pattern']
                result = arm['result']
                guard = arm.get('guard')  # Optional guard expression

                check_label = check_labels[i]
                arm_label = arm_labels[i]
                guard_label = guard_labels[i]
                arm_end_label = arm_end_labels[i]
                next_check = check_labels[i + 1] if i < num_arms - 1 else no_match_label

                pat_type = pattern.get('type') if pattern else 'Wildcard'

                # Emit check block
                self.emit(f'{check_label}:')

                if pat_type == 'Wildcard' or pat_type == 'WildcardPattern':
                    # Wildcard - check guard if present, otherwise unconditional
                    if guard:
                        self.emit(f'  br label %{guard_label}')
                    else:
                        self.emit(f'  br label %{arm_label}')
                elif pat_type == 'BindingPattern':
                    # Binding pattern - bind scrutinee to variable, always matches
                    binding_name = pattern['name']
                    local_suffix = self.local_counter
                    self.local_counter += 1
                    slot = f'%local.{binding_name}.{local_suffix}'
                    self.locals[binding_name] = slot
                    self.emit(f'  {slot} = alloca i64')
                    self.emit(f'  store i64 {scrutinee_val}, ptr {slot}')
                    if guard:
                        self.emit(f'  br label %{guard_label}')
                    else:
                        self.emit(f'  br label %{arm_label}')
                elif pat_type == 'EnumPattern':
                    # Enum pattern: check discriminant, extract bindings
                    variant_name = pattern['enum_variant']
                    bindings = pattern.get('bindings', [])
                    # Get discriminant value
                    disc_val = 0
                    for enum_name, variants in self.enums.items():
                        if variant_name in variants:
                            disc_val = variants[variant_name]
                            break
                        # Check for qualified name like Option::Some
                        if '::' in variant_name:
                            _, v = variant_name.split('::', 1)
                            if v in variants:
                                disc_val = variants[v]
                                break
                    # Scrutinee is ptr to enum, first 8 bytes is discriminant
                    ptr_temp = self.new_temp()
                    self.emit(f'  {ptr_temp} = inttoptr i64 {scrutinee_val} to ptr')
                    disc_load = self.new_temp()
                    self.emit(f'  {disc_load} = load i64, ptr {ptr_temp}')
                    cmp_temp = self.new_temp()
                    self.emit(f'  {cmp_temp} = icmp eq i64 {disc_load}, {disc_val}')
                    # If matches, extract bindings in arm_label; else go to next check
                    extract_label = f'extract{match_id}_{i}'
                    self.emit(f'  br i1 {cmp_temp}, label %{extract_label}, label %{next_check}')
                    self.emit(f'{extract_label}:')
                    # Extract payload bindings using recursive helper
                    for j, binding in enumerate(bindings):
                        binding_offset = 8 + j * 8  # Payload starts after discriminant
                        if binding.get('type') == 'BindingPattern':
                            bname = binding['name']
                            local_suffix = self.local_counter
                            self.local_counter += 1
                            slot = f'%local.{bname}.{local_suffix}'
                            self.locals[bname] = slot
                            self.emit(f'  {slot} = alloca i64')
                            gep = self.new_temp()
                            self.emit(f'  {gep} = getelementptr i8, ptr {ptr_temp}, i64 {binding_offset}')
                            val = self.new_temp()
                            self.emit(f'  {val} = load i64, ptr {gep}')
                            self.emit(f'  store i64 {val}, ptr {slot}')
                        elif binding.get('type') == 'EnumPattern':
                            # Nested enum - extract inner ptr and recurse
                            gep = self.new_temp()
                            self.emit(f'  {gep} = getelementptr i8, ptr {ptr_temp}, i64 {binding_offset}')
                            inner_val = self.new_temp()
                            self.emit(f'  {inner_val} = load i64, ptr {gep}')
                            inner_ptr = self.new_temp()
                            self.emit(f'  {inner_ptr} = inttoptr i64 {inner_val} to ptr')
                            # Recursively extract bindings from nested pattern
                            self.extract_pattern_bindings(binding, inner_ptr, 0)
                    if guard:
                        self.emit(f'  br label %{guard_label}')
                    else:
                        self.emit(f'  br label %{arm_label}')
                elif pat_type == 'TuplePattern':
                    # Tuple pattern: extract elements
                    elements = pattern.get('elements', [])
                    # For now, assume it always matches (single-arm tuple match)
                    ptr_temp = self.new_temp()
                    self.emit(f'  {ptr_temp} = inttoptr i64 {scrutinee_val} to ptr')
                    for j, elem in enumerate(elements):
                        if elem.get('type') == 'BindingPattern':
                            bname = elem['name']
                            local_suffix = self.local_counter
                            self.local_counter += 1
                            slot = f'%local.{bname}.{local_suffix}'
                            self.locals[bname] = slot
                            self.emit(f'  {slot} = alloca i64')
                            offset = j * 8
                            gep = self.new_temp()
                            self.emit(f'  {gep} = getelementptr i8, ptr {ptr_temp}, i64 {offset}')
                            val = self.new_temp()
                            self.emit(f'  {val} = load i64, ptr {gep}')
                            self.emit(f'  store i64 {val}, ptr {slot}')
                    if guard:
                        self.emit(f'  br label %{guard_label}')
                    else:
                        self.emit(f'  br label %{arm_label}')
                elif pat_type == 'StructPattern':
                    # Struct pattern: extract fields
                    struct_name = pattern.get('struct_name')
                    fields = pattern.get('fields', [])
                    ptr_temp = self.new_temp()
                    self.emit(f'  {ptr_temp} = inttoptr i64 {scrutinee_val} to ptr')
                    struct_fields = self.structs.get(struct_name, [])
                    for pf in fields:
                        fname = pf['name']
                        # Find field index
                        field_idx = 0
                        for idx, (sf_name, _) in enumerate(struct_fields):
                            if sf_name == fname:
                                field_idx = idx
                                break
                        local_suffix = self.local_counter
                        self.local_counter += 1
                        slot = f'%local.{fname}.{local_suffix}'
                        self.locals[fname] = slot
                        self.emit(f'  {slot} = alloca i64')
                        offset = field_idx * 8
                        gep = self.new_temp()
                        self.emit(f'  {gep} = getelementptr i8, ptr {ptr_temp}, i64 {offset}')
                        val = self.new_temp()
                        self.emit(f'  {val} = load i64, ptr {gep}')
                        self.emit(f'  store i64 {val}, ptr {slot}')
                    if guard:
                        self.emit(f'  br label %{guard_label}')
                    else:
                        self.emit(f'  br label %{arm_label}')
                else:
                    # Try to generate as expression for comparison
                    pat_val = self.generate_expr(pattern)
                    cmp_temp = self.new_temp()
                    self.emit(f'  {cmp_temp} = icmp eq i64 {scrutinee_val}, {pat_val}')
                    if guard:
                        self.emit(f'  br i1 {cmp_temp}, label %{guard_label}, label %{next_check}')
                    else:
                        self.emit(f'  br i1 {cmp_temp}, label %{arm_label}, label %{next_check}')

                # Guard evaluation block (if guard present)
                if guard:
                    self.emit(f'{guard_label}:')
                    guard_val = self.generate_expr(guard)
                    guard_cond = self.new_temp()
                    self.emit(f'  {guard_cond} = icmp ne i64 {guard_val}, 0')
                    self.emit(f'  br i1 {guard_cond}, label %{arm_label}, label %{next_check}')

                # Arm body
                self.emit(f'{arm_label}:')
                arm_val = self.generate_expr(result)
                self.emit(f'  br label %{arm_end_label}')

                # Arm end block
                self.emit(f'{arm_end_label}:')
                self.emit(f'  br label %{end_label}')

                arm_results.append(arm_val)

            # Add unreachable/default case for when no arm matches
            # This handles the branch from the last check when pattern doesn't match
            self.emit(f'{no_match_label}:')
            self.emit(f'  br label %{end_label}')

            # End block with phi - include no_match case with default value 0
            self.emit(f'{end_label}:')
            result_temp = self.new_temp()
            phi_parts = [f'[ {arm_results[i]}, %{arm_end_labels[i]} ]' for i in range(num_arms)]
            phi_parts.append(f'[ 0, %{no_match_label} ]')
            phi_args = ', '.join(phi_parts)
            self.emit(f'  {result_temp} = phi i64 {phi_args}')
            return result_temp

        if expr_type == 'Block':
            return self.generate_block(expr)

        if expr_type == 'StructLit':
            struct_name = expr['name']
            field_inits = expr['field_inits']
            num_fields = len(field_inits)

            # Allocate struct: malloc(num_fields * 8)
            alloc_size = num_fields * 8
            ptr_temp = self.new_temp()
            self.emit(f'  {ptr_temp} = call ptr @malloc(i64 {alloc_size})')

            # Get field order from struct definition
            struct_fields = self.structs.get(struct_name, [])

            # Initialize each field
            for field_name, field_expr in field_inits:
                # Generate field value
                field_val = self.generate_expr(field_expr)

                # Look up field offset
                field_idx = 0
                for i, (fname, ftype) in enumerate(struct_fields):
                    if fname == field_name:
                        field_idx = i
                        break
                offset = field_idx * 8

                # Store field using getelementptr + store
                gep_temp = self.new_temp()
                self.emit(f'  {gep_temp} = getelementptr i8, ptr {ptr_temp}, i64 {offset}')
                self.emit(f'  store i64 {field_val}, ptr {gep_temp}')

            # Convert ptr to i64
            result = self.new_temp()
            self.emit(f'  {result} = ptrtoint ptr {ptr_temp} to i64')
            return result

        if expr_type == 'TupleExpr':
            elements = expr['elements']
            num_elems = len(elements)
            if num_elems == 0:
                return '0'  # Unit tuple
            # Allocate tuple: malloc(num_elements * 8)
            alloc_size = num_elems * 8
            ptr_temp = self.new_temp()
            self.emit(f'  {ptr_temp} = call ptr @malloc(i64 {alloc_size})')
            # Store each element
            for i, elem_expr in enumerate(elements):
                elem_val = self.generate_expr(elem_expr)
                offset = i * 8
                gep_temp = self.new_temp()
                self.emit(f'  {gep_temp} = getelementptr i8, ptr {ptr_temp}, i64 {offset}')
                self.emit(f'  store i64 {elem_val}, ptr {gep_temp}')
            # Convert ptr to i64
            result = self.new_temp()
            self.emit(f'  {result} = ptrtoint ptr {ptr_temp} to i64')
            return result

        if expr_type == 'FieldAccess':
            obj = expr['object']
            field_name = expr['field']

            # Generate object value (pointer as i64)
            obj_val = self.generate_expr(obj)

            # Convert i64 to ptr
            ptr_temp = self.new_temp()
            self.emit(f'  {ptr_temp} = inttoptr i64 {obj_val} to ptr')

            # Find field offset by searching all structs
            field_idx = 0
            for struct_name, fields in self.structs.items():
                for i, (fname, ftype) in enumerate(fields):
                    if fname == field_name:
                        field_idx = i
                        break

            offset = field_idx * 8

            # GEP to field
            gep_temp = self.new_temp()
            self.emit(f'  {gep_temp} = getelementptr i8, ptr {ptr_temp}, i64 {offset}')

            # Load field value
            load_temp = self.new_temp()
            self.emit(f'  {load_temp} = load i64, ptr {gep_temp}')
            return load_temp

        if expr_type == 'MethodCall':
            obj = expr['object']
            method_name = expr['method']
            args = expr['args']

            # Try to get the type of the object
            type_name = None
            if obj.get('type') == 'IdentExpr':
                obj_name = obj['name']
                type_name = self.var_types.get(obj_name)

            # Generate object value (this will be the first argument)
            obj_val = self.generate_expr(obj)

            # Generate arguments
            arg_vals = [obj_val]  # Object is first argument
            for arg in args:
                arg_vals.append(self.generate_expr(arg))

            # ===== BUILTIN TYPE METHOD MAPPING =====
            # Map method calls on Vec, String, Option, Result to runtime intrinsics
            base_type = None
            if type_name:
                if type_name.startswith('Vec<'):
                    base_type = 'Vec'
                elif type_name.startswith('Option<'):
                    base_type = 'Option'
                elif type_name.startswith('Result<'):
                    base_type = 'Result'
                elif type_name == 'String':
                    base_type = 'String'

            # Special handling for Vec.get - returns Option<T>
            if base_type == 'Vec' and method_name == 'get':
                # Convert vec ptr
                vec_ptr = self.new_temp()
                self.emit(f'  {vec_ptr} = inttoptr i64 {arg_vals[0]} to ptr')
                # Call intrinsic_vec_get
                raw_result = self.new_temp()
                self.emit(f'  {raw_result} = call ptr @intrinsic_vec_get(ptr {vec_ptr}, i64 {arg_vals[1]})')
                # Check if NULL
                is_null = self.new_temp()
                self.emit(f'  {is_null} = icmp eq ptr {raw_result}, null')
                # Branch
                some_label = self.new_label('vec_get_some')
                none_label = self.new_label('vec_get_none')
                done_label = self.new_label('vec_get_done')
                self.emit(f'  br i1 {is_null}, label %{none_label}, label %{some_label}')
                # Some branch - allocate Option with tag=1, value at offset 8
                self.emit(f'{some_label}:')
                some_ptr = self.new_temp()
                self.emit(f'  {some_ptr} = call ptr @malloc(i64 16)')
                self.emit(f'  store i64 1, ptr {some_ptr}')  # tag = 1 (Some)
                some_val_ptr = self.new_temp()
                self.emit(f'  {some_val_ptr} = getelementptr i8, ptr {some_ptr}, i64 8')
                # Store the value (convert ptr to i64)
                val_i64 = self.new_temp()
                self.emit(f'  {val_i64} = ptrtoint ptr {raw_result} to i64')
                self.emit(f'  store i64 {val_i64}, ptr {some_val_ptr}')
                some_result = self.new_temp()
                self.emit(f'  {some_result} = ptrtoint ptr {some_ptr} to i64')
                self.emit(f'  br label %{done_label}')
                # None branch - allocate Option with tag=0
                self.emit(f'{none_label}:')
                none_ptr = self.new_temp()
                self.emit(f'  {none_ptr} = call ptr @malloc(i64 16)')
                self.emit(f'  store i64 0, ptr {none_ptr}')  # tag = 0 (None)
                none_result = self.new_temp()
                self.emit(f'  {none_result} = ptrtoint ptr {none_ptr} to i64')
                self.emit(f'  br label %{done_label}')
                # Done - phi
                self.emit(f'{done_label}:')
                result_temp = self.new_temp()
                self.emit(f'  {result_temp} = phi i64 [ {some_result}, %{some_label} ], [ {none_result}, %{none_label} ]')
                return result_temp

            # ===== INLINE OPTION METHODS =====
            # Handle Option<T> methods inline since they're just tag checks
            if base_type == 'Option':
                opt_ptr = self.new_temp()
                self.emit(f'  {opt_ptr} = inttoptr i64 {arg_vals[0]} to ptr')
                tag = self.new_temp()
                self.emit(f'  {tag} = load i64, ptr {opt_ptr}')

                if method_name == 'is_some':
                    # tag == 1 means Some
                    result = self.new_temp()
                    self.emit(f'  {result} = icmp eq i64 {tag}, 1')
                    result_i64 = self.new_temp()
                    self.emit(f'  {result_i64} = zext i1 {result} to i64')
                    return result_i64

                elif method_name == 'is_none':
                    # tag == 0 means None
                    result = self.new_temp()
                    self.emit(f'  {result} = icmp eq i64 {tag}, 0')
                    result_i64 = self.new_temp()
                    self.emit(f'  {result_i64} = zext i1 {result} to i64')
                    return result_i64

                elif method_name == 'unwrap':
                    # Return value at offset 8 (panic if None)
                    val_ptr = self.new_temp()
                    self.emit(f'  {val_ptr} = getelementptr i8, ptr {opt_ptr}, i64 8')
                    result = self.new_temp()
                    self.emit(f'  {result} = load i64, ptr {val_ptr}')
                    return result

                elif method_name == 'unwrap_or':
                    # Return value if Some, else default
                    default_val = arg_vals[1] if len(arg_vals) > 1 else '0'
                    is_some = self.new_temp()
                    self.emit(f'  {is_some} = icmp eq i64 {tag}, 1')
                    some_label = self.new_label('unwrap_or_some')
                    none_label = self.new_label('unwrap_or_none')
                    done_label = self.new_label('unwrap_or_done')
                    self.emit(f'  br i1 {is_some}, label %{some_label}, label %{none_label}')
                    self.emit(f'{some_label}:')
                    val_ptr = self.new_temp()
                    self.emit(f'  {val_ptr} = getelementptr i8, ptr {opt_ptr}, i64 8')
                    some_val = self.new_temp()
                    self.emit(f'  {some_val} = load i64, ptr {val_ptr}')
                    self.emit(f'  br label %{done_label}')
                    self.emit(f'{none_label}:')
                    self.emit(f'  br label %{done_label}')
                    self.emit(f'{done_label}:')
                    result = self.new_temp()
                    self.emit(f'  {result} = phi i64 [ {some_val}, %{some_label} ], [ {default_val}, %{none_label} ]')
                    return result

            # ===== INLINE RESULT METHODS =====
            # Handle Result<T,E> methods inline
            if base_type == 'Result':
                res_ptr = self.new_temp()
                self.emit(f'  {res_ptr} = inttoptr i64 {arg_vals[0]} to ptr')
                tag = self.new_temp()
                self.emit(f'  {tag} = load i64, ptr {res_ptr}')

                if method_name == 'is_ok':
                    # tag == 1 means Ok
                    result = self.new_temp()
                    self.emit(f'  {result} = icmp eq i64 {tag}, 1')
                    result_i64 = self.new_temp()
                    self.emit(f'  {result_i64} = zext i1 {result} to i64')
                    return result_i64

                elif method_name == 'is_err':
                    # tag == 0 means Err
                    result = self.new_temp()
                    self.emit(f'  {result} = icmp eq i64 {tag}, 0')
                    result_i64 = self.new_temp()
                    self.emit(f'  {result_i64} = zext i1 {result} to i64')
                    return result_i64

                elif method_name == 'unwrap':
                    # Return value at offset 8 (panic if Err)
                    val_ptr = self.new_temp()
                    self.emit(f'  {val_ptr} = getelementptr i8, ptr {res_ptr}, i64 8')
                    result = self.new_temp()
                    self.emit(f'  {result} = load i64, ptr {val_ptr}')
                    return result

                elif method_name == 'unwrap_err':
                    # Return error at offset 8 (panic if Ok)
                    val_ptr = self.new_temp()
                    self.emit(f'  {val_ptr} = getelementptr i8, ptr {res_ptr}, i64 8')
                    result = self.new_temp()
                    self.emit(f'  {result} = load i64, ptr {val_ptr}')
                    return result

                elif method_name == 'unwrap_or':
                    # Return value if Ok, else default
                    default_val = arg_vals[1] if len(arg_vals) > 1 else '0'
                    is_ok = self.new_temp()
                    self.emit(f'  {is_ok} = icmp eq i64 {tag}, 1')
                    ok_label = self.new_label('result_unwrap_or_ok')
                    err_label = self.new_label('result_unwrap_or_err')
                    done_label = self.new_label('result_unwrap_or_done')
                    self.emit(f'  br i1 {is_ok}, label %{ok_label}, label %{err_label}')
                    self.emit(f'{ok_label}:')
                    val_ptr = self.new_temp()
                    self.emit(f'  {val_ptr} = getelementptr i8, ptr {res_ptr}, i64 8')
                    ok_val = self.new_temp()
                    self.emit(f'  {ok_val} = load i64, ptr {val_ptr}')
                    self.emit(f'  br label %{done_label}')
                    self.emit(f'{err_label}:')
                    self.emit(f'  br label %{done_label}')
                    self.emit(f'{done_label}:')
                    result = self.new_temp()
                    self.emit(f'  {result} = phi i64 [ {ok_val}, %{ok_label} ], [ {default_val}, %{err_label} ]')
                    return result

            # Method mappings for builtin types
            builtin_methods = {
                ('Vec', 'push'): ('intrinsic_vec_push', ['ptr', 'ptr'], 'void'),
                ('Vec', 'len'): ('intrinsic_vec_len', ['ptr'], 'i64'),
                # Vec.get is handled specially above
                ('Vec', 'pop'): ('intrinsic_vec_pop', ['ptr'], 'ptr'),
                ('Vec', 'clear'): ('intrinsic_vec_clear', ['ptr'], 'void'),
                ('Vec', 'capacity'): ('intrinsic_vec_capacity', ['ptr'], 'i64'),
                ('Vec', 'set'): ('intrinsic_vec_set', ['ptr', 'i64', 'ptr'], 'void'),
                ('String', 'len'): ('intrinsic_string_len', ['ptr'], 'i64'),
                ('String', 'char_at'): ('intrinsic_string_char_at', ['ptr', 'i64'], 'i64'),
                ('String', 'slice'): ('intrinsic_string_slice', ['ptr', 'i64', 'i64'], 'ptr'),
                ('String', 'eq'): ('intrinsic_string_eq', ['ptr', 'ptr'], 'i64'),
                ('String', 'concat'): ('intrinsic_string_concat', ['ptr', 'ptr'], 'ptr'),
            }

            method_key = (base_type, method_name) if base_type else None
            if method_key and method_key in builtin_methods:
                intrinsic_name, param_types, ret_type = builtin_methods[method_key]
                result_temp = self.new_temp()

                # Convert arguments to proper types (i64 -> ptr where needed)
                converted_args = []
                for i, (arg_val, param_type) in enumerate(zip(arg_vals, param_types)):
                    if param_type == 'ptr':
                        conv_temp = self.new_temp()
                        self.emit(f'  {conv_temp} = inttoptr i64 {arg_val} to ptr')
                        converted_args.append((conv_temp, 'ptr'))
                    else:
                        converted_args.append((arg_val, param_type))

                args_str = ', '.join(f'{t} {v}' for v, t in converted_args)

                if ret_type == 'void':
                    self.emit(f'  call void @{intrinsic_name}({args_str})')
                    return '0'  # void methods return 0
                elif ret_type == 'ptr':
                    ptr_temp = self.new_temp()
                    self.emit(f'  {ptr_temp} = call ptr @{intrinsic_name}({args_str})')
                    # Convert ptr back to i64 for Simplex
                    self.emit(f'  {result_temp} = ptrtoint ptr {ptr_temp} to i64')
                else:
                    self.emit(f'  {result_temp} = call {ret_type} @{intrinsic_name}({args_str})')

                return result_temp

            # Build mangled function name: TypeName_methodName
            if type_name:
                callee_name = f"{type_name}_{method_name}"
            else:
                callee_name = method_name

            # Emit call
            result_temp = self.new_temp()
            args_str = ', '.join(f'i64 {v}' for v in arg_vals)
            self.emit(f'  {result_temp} = call i64 @"{callee_name}"({args_str})')
            return result_temp

        if expr_type == 'ClosureExpr':
            # Generate closure with environment capture support
            closure_name = f"__closure_{self.module_name}_{self.closure_counter}"
            self.closure_counter += 1

            params = expr['params']
            body = expr['body']

            # Find free variables (captured from enclosing scope)
            param_names = {p['name'] for p in params}
            captured_vars = self.find_free_variables(body, param_names)

            # Filter to only include variables that exist in current scope
            captured_vars = [v for v in captured_vars if v in self.locals]

            if captured_vars:
                # ===== CAPTURING CLOSURE =====
                # Closure struct layout: fn_ptr(0), env_ptr(8)
                # Environment struct: captured_var_0(0), captured_var_1(8), ...

                # Generate closure function with environment as first parameter
                params_with_env = [('__env', 'i64')] + [(p['name'], 'i64') for p in params]
                params_str = ', '.join(f"i64 %param.{name}" for name, ty in params_with_env)

                closure_lines = []
                closure_lines.append(f'; Capturing closure with {len(captured_vars)} captured vars: {captured_vars}')
                closure_lines.append(f'define i64 @"{closure_name}"({params_str}) {{')
                closure_lines.append('entry:')

                # Save current state
                saved_output = self.output
                saved_locals = self.locals
                saved_temp = self.temp_counter
                saved_label = self.label_counter
                saved_local = self.local_counter
                saved_alloca_queue = getattr(self, 'pre_alloca_queue', [])

                # Reset for closure generation
                self.output = []
                self.locals = {}
                self.temp_counter = 0
                self.label_counter = 0
                self.local_counter = 0
                self.pre_alloca_queue = []

                # Load environment pointer
                env_local = self.new_local('__env')
                self.emit(f'  {env_local} = alloca i64')
                self.emit(f'  store i64 %param.__env, ptr {env_local}')

                # Convert env to ptr
                env_ptr_load = self.new_temp()
                self.emit(f'  {env_ptr_load} = load i64, ptr {env_local}')
                env_ptr = self.new_temp()
                self.emit(f'  {env_ptr} = inttoptr i64 {env_ptr_load} to ptr')

                # Load captured variables from environment
                for i, var_name in enumerate(captured_vars):
                    offset = i * 8
                    local = self.new_local(var_name)
                    self.emit(f'  {local} = alloca i64')
                    gep = self.new_temp()
                    self.emit(f'  {gep} = getelementptr i8, ptr {env_ptr}, i64 {offset}')
                    val = self.new_temp()
                    self.emit(f'  {val} = load i64, ptr {gep}')
                    self.emit(f'  store i64 {val}, ptr {local}')
                    self.locals[var_name] = local

                # Allocate closure params
                for p in params:
                    local = self.new_local(p['name'])
                    self.emit(f'  {local} = alloca i64')
                    self.emit(f'  store i64 %param.{p["name"]}, ptr {local}')
                    self.locals[p['name']] = local

                # Generate body
                result = self.generate_expr(body)
                self.emit(f'  ret i64 {result}')
                self.emit('}')

                # Collect closure output
                closure_code = '\n'.join(closure_lines + self.output)
                self.pending_closures.append(closure_code)

                # Restore state
                self.output = saved_output
                self.locals = saved_locals
                self.temp_counter = saved_temp
                self.label_counter = saved_label
                self.local_counter = saved_local
                self.pre_alloca_queue = saved_alloca_queue

                # === Create closure struct at call site ===
                # Allocate environment struct
                env_size = len(captured_vars) * 8
                env_alloc = self.new_temp()
                self.emit(f'  {env_alloc} = call ptr @malloc(i64 {env_size})')

                # Store captured variables in environment
                for i, var_name in enumerate(captured_vars):
                    offset = i * 8
                    var_val = self.new_temp()
                    self.emit(f'  {var_val} = load i64, ptr {self.locals[var_name]}')
                    gep = self.new_temp()
                    self.emit(f'  {gep} = getelementptr i8, ptr {env_alloc}, i64 {offset}')
                    self.emit(f'  store i64 {var_val}, ptr {gep}')

                # Allocate closure struct (fn_ptr + env_ptr)
                closure_struct = self.new_temp()
                self.emit(f'  {closure_struct} = call ptr @malloc(i64 16)')

                # Store function pointer
                fn_ptr = self.new_temp()
                self.emit(f'  {fn_ptr} = ptrtoint ptr @"{closure_name}" to i64')
                self.emit(f'  store i64 {fn_ptr}, ptr {closure_struct}')

                # Store environment pointer
                env_ptr_store = self.new_temp()
                self.emit(f'  {env_ptr_store} = getelementptr i8, ptr {closure_struct}, i64 8')
                env_as_i64 = self.new_temp()
                self.emit(f'  {env_as_i64} = ptrtoint ptr {env_alloc} to i64')
                self.emit(f'  store i64 {env_as_i64}, ptr {env_ptr_store}')

                # Return closure struct as i64
                result = self.new_temp()
                self.emit(f'  {result} = ptrtoint ptr {closure_struct} to i64')
                return result

            else:
                # ===== NON-CAPTURING CLOSURE =====
                # For consistency, use same calling convention as capturing closures
                # Build param list with unused env parameter first
                params_with_env = [('__env', 'i64')] + [(p['name'], 'i64') for p in params]
                params_str = ', '.join(f"i64 %param.{name}" for name, ty in params_with_env)

                closure_lines = []
                closure_lines.append(f'; Non-capturing closure')
                closure_lines.append(f'define i64 @"{closure_name}"({params_str}) {{')
                closure_lines.append('entry:')

                # Save current state
                saved_output = self.output
                saved_locals = self.locals
                saved_temp = self.temp_counter
                saved_label = self.label_counter
                saved_local = self.local_counter
                saved_alloca_queue = getattr(self, 'pre_alloca_queue', [])

                # Reset for closure generation
                self.output = []
                self.locals = {}
                self.temp_counter = 0
                self.label_counter = 0
                self.local_counter = 0
                self.pre_alloca_queue = []

                # Allocate params
                for p in params:
                    local = self.new_local(p['name'])
                    self.emit(f'  {local} = alloca i64')
                    self.emit(f'  store i64 %param.{p["name"]}, ptr {local}')
                    self.locals[p['name']] = local

                # Generate body
                result = self.generate_expr(body)
                self.emit(f'  ret i64 {result}')
                self.emit('}')

                # Collect closure output
                closure_code = '\n'.join(closure_lines + self.output)
                self.pending_closures.append(closure_code)

                # Restore state
                self.output = saved_output
                self.locals = saved_locals
                self.temp_counter = saved_temp
                self.label_counter = saved_label
                self.local_counter = saved_local
                self.pre_alloca_queue = saved_alloca_queue

                # Create closure struct for consistency (fn_ptr + null env_ptr)
                closure_struct = self.new_temp()
                self.emit(f'  {closure_struct} = call ptr @malloc(i64 16)')

                # Store function pointer
                fn_ptr = self.new_temp()
                self.emit(f'  {fn_ptr} = ptrtoint ptr @"{closure_name}" to i64')
                self.emit(f'  store i64 {fn_ptr}, ptr {closure_struct}')

                # Store null environment pointer
                env_ptr_store = self.new_temp()
                self.emit(f'  {env_ptr_store} = getelementptr i8, ptr {closure_struct}, i64 8')
                self.emit(f'  store i64 0, ptr {env_ptr_store}')

                # Return closure struct as i64
                result = self.new_temp()
                self.emit(f'  {result} = ptrtoint ptr {closure_struct} to i64')
                return result

        if expr_type == 'AsyncClosureExpr':
            # Generate async anonymous function that returns a future
            closure_name = f"__async_closure_{self.module_name}_{self.closure_counter}"
            self.closure_counter += 1

            params = expr['params']
            body = expr['body']
            num_params = len(params)

            # Generate both the wrapper function and the poll function
            # Wrapper: creates future, stores params, returns future ptr
            # Poll: state machine that executes the body

            # Build param list for wrapper signature
            params_str = ', '.join(f"i64 %param.{p['name']}" for p in params)

            # Generate wrapper function (creates and returns future)
            wrapper_lines = []
            wrapper_lines.append(f'define i64 @"{closure_name}"({params_str}) {{')
            wrapper_lines.append('entry:')

            # Future layout: poll_fn(0), state(8), inner_future(16), result(24), params(32+)
            future_size = 32 + num_params * 8
            wrapper_lines.append(f'  %future = call ptr @malloc(i64 {future_size})')

            # Store poll function pointer at offset 0
            wrapper_lines.append(f'  %poll_fn_ptr = ptrtoint ptr @"{closure_name}_poll" to i64')
            wrapper_lines.append(f'  store i64 %poll_fn_ptr, ptr %future')

            # Store initial state at offset 8
            wrapper_lines.append(f'  %state_ptr = getelementptr i8, ptr %future, i64 8')
            wrapper_lines.append(f'  store i64 0, ptr %state_ptr')

            # Store inner_future=0 at offset 16
            wrapper_lines.append(f'  %inner_ptr = getelementptr i8, ptr %future, i64 16')
            wrapper_lines.append(f'  store i64 0, ptr %inner_ptr')

            # Store result=0 at offset 24
            wrapper_lines.append(f'  %result_ptr = getelementptr i8, ptr %future, i64 24')
            wrapper_lines.append(f'  store i64 0, ptr %result_ptr')

            # Store parameters at offset 32+
            for i, p in enumerate(params):
                offset = 32 + i * 8
                wrapper_lines.append(f'  %param_{i}_ptr = getelementptr i8, ptr %future, i64 {offset}')
                wrapper_lines.append(f'  store i64 %param.{p["name"]}, ptr %param_{i}_ptr')

            # Return future as i64
            wrapper_lines.append(f'  %future_i64 = ptrtoint ptr %future to i64')
            wrapper_lines.append(f'  ret i64 %future_i64')
            wrapper_lines.append('}')

            # Generate poll function
            poll_lines = []
            poll_lines.append(f'define i64 @"{closure_name}_poll"(i64 %future_ptr) {{')
            poll_lines.append('entry:')
            poll_lines.append('  %future = inttoptr i64 %future_ptr to ptr')

            # Save current state
            saved_output = self.output
            saved_locals = self.locals
            saved_temp = self.temp_counter
            saved_label = self.label_counter
            saved_local = self.local_counter
            saved_alloca_queue = getattr(self, 'pre_alloca_queue', [])

            # Reset for poll function generation
            self.output = []
            self.locals = {}
            self.temp_counter = 0
            self.label_counter = 0
            self.local_counter = 0
            self.pre_alloca_queue = []

            # Load parameters from future
            for i, p in enumerate(params):
                offset = 32 + i * 8
                local = self.new_local(p['name'])
                self.emit(f'  {local} = alloca i64')
                ptr_temp = self.new_temp()
                self.emit(f'  {ptr_temp} = getelementptr i8, ptr %future, i64 {offset}')
                val_temp = self.new_temp()
                self.emit(f'  {val_temp} = load i64, ptr {ptr_temp}')
                self.emit(f'  store i64 {val_temp}, ptr {local}')
                self.locals[p['name']] = local

            # Generate body
            result = self.generate_expr(body)

            # Return Ready with result
            ready_temp = self.new_temp()
            self.emit(f'  {ready_temp} = shl i64 {result}, 1')
            final_temp = self.new_temp()
            self.emit(f'  {final_temp} = or i64 {ready_temp}, 1')
            self.emit(f'  ret i64 {final_temp}')
            self.emit('}')

            # Collect poll function output
            poll_code = '\n'.join(poll_lines + self.output)
            self.pending_closures.append('\n'.join(wrapper_lines))
            self.pending_closures.append(poll_code)

            # Restore state
            self.output = saved_output
            self.locals = saved_locals
            self.temp_counter = saved_temp
            self.label_counter = saved_label
            self.local_counter = saved_local
            self.pre_alloca_queue = saved_alloca_queue

            # Return function pointer as i64 (caller will invoke to get future)
            temp = self.new_temp()
            self.emit(f'  {temp} = ptrtoint ptr @"{closure_name}" to i64')
            return temp

        if expr_type == 'AwaitExpr':
            # Await a future - proper state machine implementation for async functions
            future_val = self.generate_expr(expr['expr'])

            # Check if we're in an async function with state machine
            if hasattr(self, 'is_in_async_fn') and self.is_in_async_fn and hasattr(self, 'state_labels'):
                # Get next state index for this await
                next_state = getattr(self, 'current_await_index', 0) + 1
                self.current_await_index = next_state

                # Store inner future at offset 16
                self.emit(f'  %await_inner_ptr_{next_state} = getelementptr i8, ptr %future, i64 16')
                self.emit(f'  store i64 {future_val}, ptr %await_inner_ptr_{next_state}')

                # Save all locals to future struct before suspending
                if hasattr(self, 'async_all_locals') and hasattr(self, 'async_locals_offset'):
                    for i, local_name in enumerate(self.async_all_locals):
                        if local_name in self.locals:
                            offset = self.async_locals_offset + i * 8
                            local = self.locals[local_name]
                            save_val = self.new_temp()
                            self.emit(f'  {save_val} = load i64, ptr {local}')
                            save_ptr = self.new_temp()
                            self.emit(f'  {save_ptr} = getelementptr i8, ptr %future, i64 {offset}')
                            self.emit(f'  store i64 {save_val}, ptr {save_ptr}')

                # Update state to next state
                self.emit(f'  %await_state_ptr_{next_state} = getelementptr i8, ptr %future, i64 8')
                self.emit(f'  store i64 {next_state}, ptr %await_state_ptr_{next_state}')

                # Return Pending (0)
                self.emit(f'  ret i64 0')

                # Generate the resume state label
                if next_state < len(self.state_labels):
                    self.emit(f'{self.state_labels[next_state]}:')

                    # Load inner future from offset 16
                    inner_load_ptr = self.new_temp()
                    self.emit(f'  {inner_load_ptr} = getelementptr i8, ptr %future, i64 16')
                    inner_future = self.new_temp()
                    self.emit(f'  {inner_future} = load i64, ptr {inner_load_ptr}')

                    # Poll the inner future
                    poll_result = self.new_temp()
                    self.emit(f'  {poll_result} = call i64 @future_poll(i64 {inner_future})')

                    # Check if ready (bit 0 set = ready)
                    is_ready = self.new_temp()
                    self.emit(f'  {is_ready} = and i64 {poll_result}, 1')
                    is_pending = self.new_temp()
                    self.emit(f'  {is_pending} = icmp eq i64 {is_ready}, 0')

                    still_pending_label = self.new_label('await_pending')
                    ready_label = self.new_label('await_ready')
                    self.emit(f'  br i1 {is_pending}, label %{still_pending_label}, label %{ready_label}')

                    # Still pending - return Pending
                    self.emit(f'{still_pending_label}:')
                    self.emit(f'  ret i64 0')

                    # Ready - extract value and continue
                    self.emit(f'{ready_label}:')

                # Extract value (shift right to remove tag bit)
                value_result = self.new_temp()
                self.emit(f'  {value_result} = lshr i64 {poll_result}, 1')

                return value_result
            else:
                # Fallback: blocking poll loop for non-async context
                loop_label = self.new_label('await_loop')
                done_label = self.new_label('await_done')

                self.emit(f'  br label %{loop_label}')
                self.emit(f'{loop_label}:')

                # Poll the future
                poll_result = self.new_temp()
                self.emit(f'  {poll_result} = call i64 @future_poll(i64 {future_val})')

                # Check if ready (bit 0 set = ready)
                is_ready = self.new_temp()
                self.emit(f'  {is_ready} = and i64 {poll_result}, 1')
                is_pending = self.new_temp()
                self.emit(f'  {is_pending} = icmp eq i64 {is_ready}, 0')
                self.emit(f'  br i1 {is_pending}, label %{loop_label}, label %{done_label}')

                self.emit(f'{done_label}:')

                # Extract value (shift right to remove tag bit)
                value_result = self.new_temp()
                self.emit(f'  {value_result} = lshr i64 {poll_result}, 1')

                return value_result

        if expr_type == 'TryExpr':
            # ? operator: desugar to Result check and early return
            # Result layout: tag(0=Err, 1=Ok), value(1)
            # If Err, early return the Result; if Ok, unwrap the value

            inner_val = self.generate_expr(expr['expr'])

            # Convert i64 to ptr
            ptr_temp = self.new_temp()
            self.emit(f'  {ptr_temp} = inttoptr i64 {inner_val} to ptr')

            # Load the tag (field 0)
            tag_temp = self.new_temp()
            self.emit(f'  {tag_temp} = load i64, ptr {ptr_temp}')

            # Check if tag == 0 (Err)
            is_err_temp = self.new_temp()
            self.emit(f'  {is_err_temp} = icmp eq i64 {tag_temp}, 0')

            # Generate labels
            err_label = self.new_label('try_err')
            ok_label = self.new_label('try_ok')

            # Branch: if Err goto err_label, else goto ok_label
            self.emit(f'  br i1 {is_err_temp}, label %{err_label}, label %{ok_label}')

            # Error path: early return the Result
            self.emit(f'{err_label}:')
            self.emit(f'  ret i64 {inner_val}')

            # Ok path: unwrap the value (field 1, offset 8)
            self.emit(f'{ok_label}:')
            gep_temp = self.new_temp()
            self.emit(f'  {gep_temp} = getelementptr i8, ptr {ptr_temp}, i64 8')
            value_temp = self.new_temp()
            self.emit(f'  {value_temp} = load i64, ptr {gep_temp}')

            return value_temp

        if expr_type == 'SpawnExpr':
            # spawn ActorName -> ActorName_new()
            actor_name = expr['actor_name']
            temp = self.new_temp()
            self.emit(f'  {temp} = call i64 @"{actor_name}_new"()')
            return temp

        if expr_type == 'InferExpr':
            # infer(prompt) -> intrinsic_ai_infer(self.__model, prompt, self.__temperature)
            prompt = expr['prompt']
            options = expr['options']

            # Generate prompt string
            prompt_val = self.generate_expr(prompt)
            prompt_ptr = self.new_temp()
            self.emit(f'  {prompt_ptr} = inttoptr i64 {prompt_val} to ptr')

            # Get model and temperature from specialist's state (if in specialist context)
            if hasattr(self, 'current_specialist') and self.current_specialist:
                # Load model pointer from self (first field)
                self_local = self.locals.get('self')
                if self_local:
                    self_ptr = self.new_temp()
                    self.emit(f'  {self_ptr} = load i64, ptr {self_local}')
                    ptr_temp = self.new_temp()
                    self.emit(f'  {ptr_temp} = inttoptr i64 {self_ptr} to ptr')

                    # Model is at offset 0
                    model_load = self.new_temp()
                    self.emit(f'  {model_load} = load i64, ptr {ptr_temp}')
                    model_ptr = self.new_temp()
                    self.emit(f'  {model_ptr} = inttoptr i64 {model_load} to ptr')

                    # Temperature is at offset 8
                    temp_gep = self.new_temp()
                    self.emit(f'  {temp_gep} = getelementptr i8, ptr {ptr_temp}, i64 8')
                    temp_load = self.new_temp()
                    self.emit(f'  {temp_load} = load i64, ptr {temp_gep}')

                    # Call AI intrinsic
                    result_ptr = self.new_temp()
                    self.emit(f'  {result_ptr} = call ptr @intrinsic_ai_infer(ptr {model_ptr}, ptr {prompt_ptr}, i64 {temp_load})')
                    result = self.new_temp()
                    self.emit(f'  {result} = ptrtoint ptr {result_ptr} to i64')
                    return result

            # Not in specialist context - use default model
            model_label = self.add_string_constant("default")
            result_ptr = self.new_temp()
            self.emit(f'  {result_ptr} = call ptr @intrinsic_ai_infer(ptr {model_label}, ptr {prompt_ptr}, i64 70)')
            result = self.new_temp()
            self.emit(f'  {result} = ptrtoint ptr {result_ptr} to i64')
            return result

        if expr_type == 'SendExpr':
            # send(target, MessageName(args)) -> ActorName_handle_MessageName(target, args)
            # For synchronous actors, this is a direct call
            target = expr['target']
            msg_name = expr['message_name']
            args = expr['args']

            target_val = self.generate_expr(target)
            arg_vals = [self.generate_expr(a) for a in args]

            # Need to figure out the actor type from target
            # For now, try to look it up from var_types or actors
            actor_name = None
            if target.get('type') == 'IdentExpr':
                target_name = target['name']
                if target_name == 'self' and hasattr(self, 'current_actor') and self.current_actor:
                    actor_name = self.current_actor
                else:
                    actor_name = self.var_types.get(target_name)

            # If we know the actor type, call its handler
            if actor_name and actor_name in self.actors:
                all_args = [target_val] + arg_vals
                args_str = ', '.join(f'i64 {v}' for v in all_args)
                # Check if handler returns void or i64
                handler_ret = 'void'
                for h in self.actors[actor_name]['handlers']:
                    if h['name'] == msg_name:
                        handler_ret = self.type_to_llvm(h['return_type'])
                        break
                if handler_ret == 'void':
                    self.emit(f'  call void @"{actor_name}_handle_{msg_name}"({args_str})')
                    return '0'
                else:
                    temp = self.new_temp()
                    self.emit(f'  {temp} = call i64 @"{actor_name}_handle_{msg_name}"({args_str})')
                    return temp
            else:
                # Unknown actor type, try generic call
                all_args = [target_val] + arg_vals
                args_str = ', '.join(f'i64 {v}' for v in all_args)
                self.emit(f'  ; send to unknown actor type')
                return '0'

        if expr_type == 'AskExpr':
            # ask(target, MessageName(args)) -> ActorName_handle_MessageName(target, args)
            # Same as send for synchronous actors, but always returns a value
            target = expr['target']
            msg_name = expr['message_name']
            args = expr['args']

            target_val = self.generate_expr(target)
            arg_vals = [self.generate_expr(a) for a in args]

            # Try to determine actor type
            actor_name = None
            if target.get('type') == 'IdentExpr':
                target_name = target['name']
                if target_name == 'self' and hasattr(self, 'current_actor') and self.current_actor:
                    actor_name = self.current_actor
                else:
                    actor_name = self.var_types.get(target_name)

            if actor_name and actor_name in self.actors:
                all_args = [target_val] + arg_vals
                args_str = ', '.join(f'i64 {v}' for v in all_args)
                temp = self.new_temp()
                self.emit(f'  {temp} = call i64 @"{actor_name}_handle_{msg_name}"({args_str})')
                return temp
            else:
                # Unknown actor type
                self.emit(f'  ; ask to unknown actor type')
                return '0'

        return '0'


def main():
    import os

    if len(sys.argv) < 2:
        print("Usage: stage0.py <input.sx> [input2.sx ...]")
        sys.exit(1)

    input_files = sys.argv[1:]

    # First pass: parse all files and collect enums and structs
    # Initialize with built-in enums: Option<T> and Result<T,E>
    all_enums = {
        'Option': {'None': 0, 'Some': 1},
        'Result': {'Err': 0, 'Ok': 1},
    }
    all_structs = {}
    parsed_modules = []

    for input_file in input_files:
        with open(input_file, 'r') as f:
            source = f.read()

        module_name = os.path.basename(input_file).replace('.sx', '').replace('-', '_')

        # Lex and parse
        lexer = Lexer(source)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        items = parser.parse_program()

        # Collect enums and structs from this module
        for item in items:
            if item['type'] == 'EnumDef':
                variants = item['variants']
                if variants and isinstance(variants[0], dict):
                    all_enums[item['name']] = {v['name']: i for i, v in enumerate(variants)}
                else:
                    all_enums[item['name']] = {v: i for i, v in enumerate(variants)}
            elif item['type'] == 'StructDef':
                all_structs[item['name']] = item['fields']

        parsed_modules.append((input_file, module_name, items))

    # Second pass: generate code for each module with shared enums and structs
    for input_file, module_name, items in parsed_modules:
        codegen = CodeGen()
        codegen.module_name = module_name
        codegen.enums = all_enums.copy()  # Pre-populate with all enums
        codegen.structs = all_structs.copy()  # Pre-populate with all structs
        llvm_ir = codegen.generate(items)

        # Output
        output_file = input_file.replace('.sx', '.ll')
        with open(output_file, 'w') as f:
            f.write(llvm_ir)

        print(f"Generated {output_file}")
        print(f"Items: {len(items)}")
        for item in items:
            if item['type'] == 'FnDef':
                print(f"  fn {item['name']}")
            elif item['type'] == 'EnumDef':
                print(f"  enum {item['name']}")
            elif item['type'] == 'StructDef':
                print(f"  struct {item['name']}")
            elif item['type'] == 'ImplDef':
                print(f"  impl {item['type_name']}")


if __name__ == '__main__':
    main()
