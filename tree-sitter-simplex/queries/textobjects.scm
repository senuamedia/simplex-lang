; Tree-sitter textobject queries for Simplex
; Used by editors for motions like "select function", "select class", etc.

; =============================================================================
; Functions
; =============================================================================

; Function definitions (inner = body, outer = whole function)
(function_definition
  body: (block) @function.inner) @function.outer

; Function signatures in traits
(function_signature) @function.outer

; Receive handlers in actors
(receive_handler
  (block) @function.inner) @function.outer

; Closures
(closure_expression
  (block) @function.inner) @function.outer

; =============================================================================
; Classes/Types
; =============================================================================

; Struct definitions
(struct_definition
  (struct_body) @class.inner) @class.outer

; Enum definitions
(enum_definition
  (enum_body) @class.inner) @class.outer

; Trait definitions
(trait_definition
  (trait_body) @class.inner) @class.outer

; Impl blocks
(impl_block
  (impl_body) @class.inner) @class.outer

; Actor definitions
(actor_definition
  (actor_body) @class.inner) @class.outer

; Specialist definitions
(specialist_definition
  (specialist_body) @class.inner) @class.outer

; Hive definitions
(hive_definition
  (hive_body) @class.inner) @class.outer

; =============================================================================
; Parameters/Arguments
; =============================================================================

; Function parameters
(parameters
  (parameter) @parameter.inner) @parameter.outer

; Function arguments
(arguments
  (_) @parameter.inner) @parameter.outer

; Closure parameters
(closure_parameters
  (closure_parameter) @parameter.inner) @parameter.outer

; Generic parameters
(generic_parameters
  (generic_parameter) @parameter.inner) @parameter.outer

; Type arguments
(type_arguments
  (_) @parameter.inner) @parameter.outer

; =============================================================================
; Blocks/Statements
; =============================================================================

; Block expressions
(block) @block.outer

; If expressions
(if_expression
  consequence: (block) @block.inner) @conditional.outer

(if_expression
  alternative: (block) @block.inner)

; Match expressions
(match_expression) @conditional.outer

(match_arm
  body: (_) @block.inner) @block.outer

; For loops
(for_expression
  body: (block) @loop.inner) @loop.outer

; While loops
(while_expression
  body: (block) @loop.inner) @loop.outer

; Loop expressions
(loop_expression
  body: (block) @loop.inner) @loop.outer

; =============================================================================
; Comments
; =============================================================================

(line_comment) @comment.outer
(block_comment) @comment.outer

; =============================================================================
; Calls
; =============================================================================

(call_expression
  (arguments) @call.inner) @call.outer

(method_call_expression
  (arguments) @call.inner) @call.outer

; =============================================================================
; Returns/Assignments
; =============================================================================

(return_statement
  (_) @return.inner) @return.outer

(assignment_expression
  right: (_) @assignment.inner) @assignment.outer

(let_statement
  value: (_) @assignment.inner) @assignment.outer

; =============================================================================
; Numbers/Strings
; =============================================================================

(integer_literal) @number.inner @number.outer
(float_literal) @number.inner @number.outer

(string_literal) @string.inner @string.outer
(fstring_literal) @string.inner @string.outer
(char_literal) @string.inner @string.outer

; =============================================================================
; Scopename (for statusline)
; =============================================================================

(function_definition
  name: (identifier) @scopename)

(struct_definition
  name: (type_identifier) @scopename)

(enum_definition
  name: (type_identifier) @scopename)

(trait_definition
  name: (type_identifier) @scopename)

(impl_block
  type: (type) @scopename)

(actor_definition
  name: (type_identifier) @scopename)

(specialist_definition
  name: (type_identifier) @scopename)
