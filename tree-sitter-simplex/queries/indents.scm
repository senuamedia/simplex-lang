; Tree-sitter indentation queries for Simplex
; Used by editors to automatically indent code

; =============================================================================
; Indent Triggers
; =============================================================================

; Opening braces increase indent
[
  (block)
  (struct_body)
  (enum_body)
  (trait_body)
  (impl_body)
  (mod_body)
  (actor_body)
  (specialist_body)
  (hive_body)
  (anima_body)
  (extern_body)
] @indent.begin

; Closing braces decrease indent
(block "}" @indent.end)
(struct_body "}" @indent.end)
(enum_body "}" @indent.end)
(trait_body "}" @indent.end)
(impl_body "}" @indent.end)
(mod_body "}" @indent.end)
(actor_body "}" @indent.end)
(specialist_body "}" @indent.end)
(hive_body "}" @indent.end)
(anima_body "}" @indent.end)
(extern_body "}" @indent.end)

; Match expression arms
(match_expression
  "{" @indent.begin
  "}" @indent.end)

; Array and tuple literals with multiple elements
(array_expression
  "[" @indent.begin
  "]" @indent.end)

; Function parameters spanning multiple lines
(parameters
  "(" @indent.begin
  ")" @indent.end)

; Function arguments spanning multiple lines
(arguments
  "(" @indent.begin
  ")" @indent.end)

; Generic parameters
(generic_parameters
  "<" @indent.begin
  ">" @indent.end)

; Type arguments
(type_arguments
  "<" @indent.begin
  ">" @indent.end)

; Struct expressions
(struct_expression
  "{" @indent.begin
  "}" @indent.end)

; Tuple types and expressions
(tuple_type
  "(" @indent.begin
  ")" @indent.end)

(tuple_expression
  "(" @indent.begin
  ")" @indent.end)

; Use groups
(use_group
  "{" @indent.begin
  "}" @indent.end)

; =============================================================================
; Continuation Indents
; =============================================================================

; Binary expressions that span multiple lines
(binary_expression) @indent.auto

; Chained method calls
(method_call_expression) @indent.auto

; Long return statements
(return_statement) @indent.auto

; Let bindings with long expressions
(let_statement) @indent.auto

; =============================================================================
; Special Cases
; =============================================================================

; Match arms - align body with pattern
(match_arm
  "=>" @indent.branch)

; Else branches align with if
(if_expression
  "else" @indent.branch)

; Where clauses indent from function
(where_clause) @indent.begin

; =============================================================================
; Alignment
; =============================================================================

; Comments should maintain their indentation
(line_comment) @indent.auto
(block_comment) @indent.auto

; Attributes should not affect indentation
(attribute) @indent.auto

; =============================================================================
; Dedent Triggers
; =============================================================================

; Keywords that typically dedent
[
  "else"
  "}"
  "]"
  ")"
] @indent.dedent

; =============================================================================
; Zero Indent (Top Level)
; =============================================================================

; Top-level items should have no indent
[
  (function_definition)
  (struct_definition)
  (enum_definition)
  (trait_definition)
  (impl_block)
  (type_alias)
  (const_declaration)
  (static_declaration)
  (use_declaration)
  (mod_declaration)
  (extern_block)
  (actor_definition)
  (specialist_definition)
  (hive_definition)
  (anima_definition)
] @indent.zero
