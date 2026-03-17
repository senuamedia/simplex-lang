; Tree-sitter local scope queries for Simplex
; Used for semantic highlighting and scope-aware features

; =============================================================================
; Scope Definitions
; =============================================================================

; Function body creates a new scope
(function_definition
  body: (block) @local.scope)

; Closure creates a new scope
(closure_expression) @local.scope

; Block expressions create new scopes
(block) @local.scope

; For loops create a scope for the pattern binding
(for_expression) @local.scope

; While let loops create a scope
(while_expression) @local.scope

; If let expressions create a scope
(if_expression) @local.scope

; Match arms create scopes for pattern bindings
(match_arm) @local.scope

; Impl blocks create a scope
(impl_block) @local.scope

; Trait definitions create a scope
(trait_definition) @local.scope

; Actor definitions create a scope
(actor_definition) @local.scope

; Receive handlers create a scope
(receive_handler) @local.scope

; Module bodies create scopes
(mod_body) @local.scope

; =============================================================================
; Definition Sites
; =============================================================================

; Function definitions
(function_definition
  name: (identifier) @local.definition.function)

; Function parameters
(typed_parameter
  name: (identifier) @local.definition.parameter)

; Closure parameters
(closure_parameter
  name: (identifier) @local.definition.parameter)

; Self parameter
(self_parameter) @local.definition.parameter

; Let bindings
(let_statement
  pattern: (pattern
    (identifier_pattern
      (identifier) @local.definition.variable)))

; For loop pattern bindings
(for_expression
  pattern: (pattern
    (identifier_pattern
      (identifier) @local.definition.variable)))

; Match arm pattern bindings
(match_arm
  pattern: (pattern
    (identifier_pattern
      (identifier) @local.definition.variable)))

; Struct definitions
(struct_definition
  name: (type_identifier) @local.definition.type)

; Enum definitions
(enum_definition
  name: (type_identifier) @local.definition.type)

; Enum variants
(enum_variant
  name: (identifier) @local.definition.constant)

; Trait definitions
(trait_definition
  name: (type_identifier) @local.definition.type)

; Type aliases
(type_alias
  name: (type_identifier) @local.definition.type)

; Generic parameters
(generic_parameter
  name: (identifier) @local.definition.type)

; Constants
(const_declaration
  name: (identifier) @local.definition.constant)

; Static variables
(static_declaration
  name: (identifier) @local.definition.variable)

; Actor definitions
(actor_definition
  name: (type_identifier) @local.definition.type)

; Actor fields
(actor_field
  name: (identifier) @local.definition.field)

; Struct fields
(struct_field
  name: (field_identifier) @local.definition.field)

; Specialist definitions
(specialist_definition
  name: (type_identifier) @local.definition.type)

; Hive definitions
(hive_definition
  name: (type_identifier) @local.definition.type)

; Anima definitions
(anima_definition
  name: (type_identifier) @local.definition.type)

; Module declarations
(mod_declaration
  name: (identifier) @local.definition.module)

; =============================================================================
; Reference Sites
; =============================================================================

; Identifiers are references
(identifier) @local.reference

; Type identifiers in type positions
(type_identifier) @local.reference

; Path segments are references
(path_segment
  (identifier) @local.reference)

; Field access
(field_expression
  field: (identifier) @local.reference)

; Method calls
(method_call_expression
  method: (identifier) @local.reference)

; =============================================================================
; Import Handling
; =============================================================================

; Use declarations bring names into scope
(use_declaration
  (use_tree
    (use_path
      (identifier) @local.definition.import)))
