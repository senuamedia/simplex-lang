; Tree-sitter syntax highlighting queries for Simplex
; Compatible with Neovim, Helix, Zed, and other editors

; =============================================================================
; Comments
; =============================================================================

(line_comment) @comment
(block_comment) @comment
; doc_comment is handled by line_comment since /// is a subset of //

; =============================================================================
; Keywords
; =============================================================================

; Function-related keywords
[
  "fn"
  "async"
  "await"
] @keyword.function

; Control flow keywords
[
  "if"
  "else"
  "match"
  "for"
  "while"
  "loop"
  "break"
  "continue"
  "return"
] @keyword.control

; Declaration keywords
[
  "let"
  "var"
  "mut"
  "const"
  "static"
  "type"
] @keyword.storage

; Type definition keywords
[
  "struct"
  "enum"
  "trait"
  "impl"
] @keyword.type

; Module keywords
[
  "mod"
  "use"
  "extern"
  "as"
  "in"
] @keyword.import

; Actor/concurrency keywords (Simplex-specific)
[
  "actor"
  "receive"
  "spawn"
  "send"
] @keyword.coroutine

; AI-native keywords (Simplex-specific)
[
  "specialist"
  "hive"
  "anima"
  "infer"
] @keyword.special

; Other keywords
[
  "where"
  "dyn"
  "move"
  "self"
] @keyword

; Self type
(self_type) @type.builtin

; Visibility
(visibility) @keyword

; =============================================================================
; Operators
; =============================================================================

; Comparison operators
[
  "=="
  "!="
  "<"
  ">"
  "<="
  ">="
] @operator

; Arithmetic operators
[
  "+"
  "-"
  "*"
  "/"
  "%"
] @operator

; Logical operators
[
  "&&"
  "||"
  "!"
] @operator

; Bitwise operators
[
  "&"
  "|"
  "^"
  "~"
  "<<"
  ">>"
] @operator

; Assignment operators
[
  "="
  "+="
  "-="
  "*="
  "/="
  "%="
  "&="
  "|="
  "^="
  "<<="
  ">>="
] @operator

; Other operators
[
  "->"
  "=>"
  "::"
  ".."
  "..="
  "?"
] @operator

; =============================================================================
; Punctuation
; =============================================================================

[
  "("
  ")"
] @punctuation.bracket

[
  "["
  "]"
] @punctuation.bracket

[
  "{"
  "}"
] @punctuation.bracket

[
  ","
  ";"
  ":"
  "."
] @punctuation.delimiter

[
  "#"
] @punctuation.special

; =============================================================================
; Literals
; =============================================================================

(integer_literal) @number
(float_literal) @number.float

(string_literal) @string
(fstring_literal) @string
(char_literal) @character
(escape_sequence) @string.escape

(boolean_literal) @boolean

; =============================================================================
; Types
; =============================================================================

(primitive_type) @type.builtin

(type_identifier) @type

(generic_type
  (path) @type)

(self_type) @type.builtin

; Lifetime
(lifetime) @label

; =============================================================================
; Functions
; =============================================================================

; Function definitions
(function_definition
  name: (identifier) @function)

; Function signatures in traits
(function_signature
  name: (identifier) @function)

; Function calls
(call_expression
  function: (identifier) @function.call)

(call_expression
  function: (path
    (path_segment
      (identifier) @function.call) .))

; Method calls
(method_call_expression
  method: (identifier) @function.method.call)

; =============================================================================
; Variables and Parameters
; =============================================================================

; Parameter names
(typed_parameter
  name: (identifier) @variable.parameter)

(closure_parameter
  name: (identifier) @variable.parameter)

(self_parameter) @variable.builtin

; Variable bindings
(let_statement
  pattern: (pattern
    (identifier_pattern
      (identifier) @variable)))

; For loop pattern
(for_expression
  pattern: (pattern
    (identifier_pattern
      (identifier) @variable)))

; Field names in struct definitions
(struct_field
  name: (field_identifier) @property)

; Field access
(field_expression
  field: (identifier) @property)

; =============================================================================
; Structs, Enums, Traits
; =============================================================================

; Struct definition name
(struct_definition
  name: (type_identifier) @type.definition)

; Enum definition name
(enum_definition
  name: (type_identifier) @type.definition)

; Enum variants
(enum_variant
  name: (identifier) @constant)

; Trait definition name
(trait_definition
  name: (type_identifier) @type.definition)

; Impl block types
(impl_block
  trait: (type) @type
  type: (type) @type)

(impl_block
  type: (type) @type)

; =============================================================================
; Actors (Simplex-specific)
; =============================================================================

(actor_definition
  name: (type_identifier) @type.definition)

(receive_handler
  message: (identifier) @function)

; =============================================================================
; AI Features (Simplex-specific)
; =============================================================================

(specialist_definition
  name: (type_identifier) @type.definition)

(hive_definition
  name: (type_identifier) @type.definition)

(anima_definition
  name: (type_identifier) @type.definition)

(specialist_config
  key: (identifier) @property)

(hive_config
  key: (identifier) @property)

(anima_config
  key: (identifier) @property)

; =============================================================================
; Modules and Imports
; =============================================================================

(mod_declaration
  name: (identifier) @module)

(use_declaration
  (use_tree
    (use_path) @module))

; =============================================================================
; Attributes
; =============================================================================

(attribute
  (attribute_content
    (identifier) @attribute))


; =============================================================================
; Type Aliases and Constants
; =============================================================================

(type_alias
  name: (type_identifier) @type.definition)

(const_declaration
  name: (identifier) @constant)

(static_declaration
  name: (identifier) @constant)

; =============================================================================
; Generics
; =============================================================================

(generic_parameter
  name: (identifier) @type.parameter)

; =============================================================================
; Paths
; =============================================================================

; Path segments that are types (capitalized)
(path
  (path_segment
    (identifier) @type)
  (#match? @type "^[A-Z]"))

; Path segments that are modules/namespaces (lowercase)
(path
  (path_segment
    (identifier) @module)
  (#match? @module "^[a-z]"))

; =============================================================================
; Struct Expressions
; =============================================================================

(struct_expression
  name: (path) @type)

(struct_expression_field
  name: (identifier) @property)

; =============================================================================
; Match Expressions
; =============================================================================

; Enum patterns in match arms
(enum_pattern
  (path) @type)

(field_pattern
  name: (identifier) @property)

; =============================================================================
; Special identifiers
; =============================================================================

; Standard library types
((identifier) @type.builtin
  (#any-of? @type.builtin
    "Option"
    "Result"
    "Ok"
    "Err"
    "Some"
    "None"
    "Vec"
    "String"
    "HashMap"
    "HashSet"
    "Box"
    "Rc"
    "Arc"
    "RefCell"
    "Cell"
    "Mutex"
    "RwLock"))

; Standard library functions
((identifier) @function.builtin
  (#any-of? @function.builtin
    "print"
    "println"
    "print_i64"
    "format"
    "panic"
    "assert"
    "assert_eq"
    "assert_ne"
    "debug_assert"
    "todo"
    "unimplemented"
    "unreachable"))

; Built-in macros (end with !)
((identifier) @function.macro
  (#match? @function.macro "!$"))
