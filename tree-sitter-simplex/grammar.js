/// Tree-sitter grammar for the Simplex programming language
/// https://github.com/senuamedia/simplex-lang
///
/// Simplex is a systems programming language with:
/// - Rust-like syntax (fn, let, struct, enum, impl, trait)
/// - Actor model concurrency (actor, receive, spawn, send)
/// - AI-native features (specialist, hive, infer, anima)
/// - Async/await support
/// - Pattern matching with match expressions

// Numeric precedence constants (higher = binds tighter)
const PREC = {
  assign: -2,
  range: -1,
  or: 1,
  and: 2,
  compare: 3,
  bitwise_or: 4,
  bitwise_xor: 5,
  bitwise_and: 6,
  shift: 7,
  additive: 8,
  multiplicative: 9,
  cast: 10,
  unary: 11,
  call: 12,
  field: 13,
};

module.exports = grammar({
  name: 'simplex',

  // Tokens that can appear anywhere (whitespace, comments)
  extras: $ => [
    /\s/,
    $.line_comment,
    $.block_comment,
  ],

  // Handle conflicts in the grammar
  conflicts: $ => [
    [$._expression, $.struct_expression],
    [$.tuple_type, $.unit_type],
    [$._expression, $.path_segment],
    [$._expression, $.closure_expression],
    [$.struct_pattern, $.enum_pattern],
    [$.path_segment, $._type_identifier],
    [$.identifier_pattern, $._type_identifier],
    [$._expression, $._type_identifier],
  ],

  // Inline rules for performance
  inline: $ => [
    $._field_identifier,
  ],

  // Word token for keyword extraction
  word: $ => $.identifier,

  rules: {
    // =========================================================================
    // Source File (Entry Point)
    // =========================================================================

    source_file: $ => repeat($._item),

    _item: $ => choice(
      $.function_definition,
      $.struct_definition,
      $.enum_definition,
      $.trait_definition,
      $.impl_block,
      $.type_alias,
      $.const_declaration,
      $.static_declaration,
      $.use_declaration,
      $.mod_declaration,
      $.extern_block,
      $.actor_definition,
      $.specialist_definition,
      $.hive_definition,
      $.anima_definition,
    ),

    // =========================================================================
    // Visibility
    // =========================================================================

    visibility: $ => 'pub',

    // =========================================================================
    // Attributes
    // =========================================================================

    attribute: $ => seq(
      '#',
      '[',
      $.attribute_content,
      ']',
    ),

    outer_attribute: $ => seq(
      '#',
      '!',
      '[',
      $.attribute_content,
      ']',
    ),

    attribute_content: $ => seq(
      $.identifier,
      optional($.attribute_arguments),
    ),

    attribute_arguments: $ => seq(
      '(',
      optional(sepBy(',', $._attribute_arg)),
      optional(','),
      ')',
    ),

    _attribute_arg: $ => choice(
      $.identifier,
      $.string_literal,
      $.integer_literal,
      seq($.identifier, '=', choice($.string_literal, $.integer_literal, $.identifier)),
    ),

    // =========================================================================
    // Function Definitions
    // =========================================================================

    function_definition: $ => seq(
      repeat($.attribute),
      optional($.visibility),
      optional('async'),
      'fn',
      field('name', $.identifier),
      optional($.generic_parameters),
      $.parameters,
      optional(seq('->', field('return_type', $.type))),
      optional($.where_clause),
      field('body', $.block),
    ),

    parameters: $ => seq(
      '(',
      optional(sepBy(',', $.parameter)),
      optional(','),
      ')',
    ),

    parameter: $ => choice(
      $.self_parameter,
      $.typed_parameter,
    ),

    self_parameter: $ => seq(
      optional('&'),
      optional('mut'),
      'self',
    ),

    typed_parameter: $ => seq(
      optional('mut'),
      field('name', $.identifier),
      ':',
      field('type', $.type),
    ),

    // =========================================================================
    // Generic Parameters
    // =========================================================================

    generic_parameters: $ => seq(
      '<',
      sepBy(',', $.generic_parameter),
      optional(','),
      '>',
    ),

    generic_parameter: $ => seq(
      field('name', $.identifier),
      optional(seq(':', $.trait_bounds)),
    ),

    trait_bounds: $ => sepBy1('+', $.type),

    where_clause: $ => prec.left(seq(
      'where',
      sepBy1(',', $.where_predicate),
      optional(','),
    )),

    where_predicate: $ => seq(
      $.type,
      ':',
      $.trait_bounds,
    ),

    // =========================================================================
    // Struct Definitions
    // =========================================================================

    struct_definition: $ => seq(
      repeat($.attribute),
      optional($.visibility),
      'struct',
      field('name', $._type_identifier),
      optional($.generic_parameters),
      optional($.where_clause),
      choice(
        $.struct_body,
        $.tuple_struct_body,
        ';',
      ),
    ),

    struct_body: $ => seq(
      '{',
      optional(sepBy(',', $.struct_field)),
      optional(','),
      '}',
    ),

    struct_field: $ => seq(
      repeat($.attribute),
      optional($.visibility),
      field('name', $._field_identifier),
      ':',
      field('type', $.type),
    ),

    tuple_struct_body: $ => seq(
      '(',
      optional(sepBy(',', $.tuple_struct_field)),
      optional(','),
      ')',
      ';',
    ),

    tuple_struct_field: $ => seq(
      optional($.visibility),
      $.type,
    ),

    // =========================================================================
    // Enum Definitions
    // =========================================================================

    enum_definition: $ => seq(
      repeat($.attribute),
      optional($.visibility),
      'enum',
      field('name', $._type_identifier),
      optional($.generic_parameters),
      optional($.where_clause),
      $.enum_body,
    ),

    enum_body: $ => seq(
      '{',
      optional(sepBy(',', $.enum_variant)),
      optional(','),
      '}',
    ),

    enum_variant: $ => seq(
      repeat($.attribute),
      field('name', $.identifier),
      optional(choice(
        $.enum_variant_tuple,
        $.enum_variant_struct,
        seq('=', $._expression),
      )),
    ),

    enum_variant_tuple: $ => seq(
      '(',
      optional(sepBy(',', $.type)),
      optional(','),
      ')',
    ),

    enum_variant_struct: $ => seq(
      '{',
      optional(sepBy(',', $.struct_field)),
      optional(','),
      '}',
    ),

    // =========================================================================
    // Trait Definitions
    // =========================================================================

    trait_definition: $ => seq(
      repeat($.attribute),
      optional($.visibility),
      'trait',
      field('name', $._type_identifier),
      optional($.generic_parameters),
      optional(seq(':', $.trait_bounds)),
      optional($.where_clause),
      $.trait_body,
    ),

    trait_body: $ => seq(
      '{',
      repeat($.trait_item),
      '}',
    ),

    trait_item: $ => choice(
      $.function_signature,
      $.function_definition,
      $.type_alias_signature,
      $.const_declaration,
    ),

    function_signature: $ => seq(
      repeat($.attribute),
      optional('async'),
      'fn',
      field('name', $.identifier),
      optional($.generic_parameters),
      $.parameters,
      optional(seq('->', field('return_type', $.type))),
      optional($.where_clause),
      ';',
    ),

    type_alias_signature: $ => seq(
      'type',
      field('name', $._type_identifier),
      optional(seq(':', $.trait_bounds)),
      ';',
    ),

    // =========================================================================
    // Impl Blocks
    // =========================================================================

    impl_block: $ => seq(
      repeat($.attribute),
      'impl',
      optional($.generic_parameters),
      optional(seq(
        field('trait', $.type),
        'for',
      )),
      field('type', $.type),
      optional($.where_clause),
      $.impl_body,
    ),

    impl_body: $ => seq(
      '{',
      repeat(choice(
        $.function_definition,
        $.type_alias,
        $.const_declaration,
      )),
      '}',
    ),

    // =========================================================================
    // Type Alias
    // =========================================================================

    type_alias: $ => seq(
      repeat($.attribute),
      optional($.visibility),
      'type',
      field('name', $._type_identifier),
      optional($.generic_parameters),
      '=',
      field('type', $.type),
      ';',
    ),

    // =========================================================================
    // Constants and Statics
    // =========================================================================

    const_declaration: $ => seq(
      repeat($.attribute),
      optional($.visibility),
      'const',
      field('name', $.identifier),
      ':',
      field('type', $.type),
      '=',
      field('value', $._expression),
      ';',
    ),

    static_declaration: $ => seq(
      repeat($.attribute),
      optional($.visibility),
      'static',
      optional('mut'),
      field('name', $.identifier),
      ':',
      field('type', $.type),
      '=',
      field('value', $._expression),
      ';',
    ),

    // =========================================================================
    // Use Declarations
    // =========================================================================

    use_declaration: $ => seq(
      repeat($.attribute),
      optional($.visibility),
      'use',
      $.use_tree,
      ';',
    ),

    use_tree: $ => choice(
      seq($.use_path, optional(seq('as', $.identifier))),
      seq($.use_path, '::', '*'),
      seq($.use_path, '::', $.use_group),
      '*',
    ),

    // Use paths: one or more identifiers separated by ::
    use_path: $ => prec.right(seq(
      $.identifier,
      repeat(seq('::', $.identifier)),
    )),

    use_group: $ => seq(
      '{',
      optional(sepBy(',', $.use_tree)),
      optional(','),
      '}',
    ),

    // =========================================================================
    // Module Declarations
    // =========================================================================

    mod_declaration: $ => seq(
      repeat($.attribute),
      optional($.visibility),
      'mod',
      field('name', $.identifier),
      choice(
        ';',
        $.mod_body,
      ),
    ),

    mod_body: $ => seq(
      '{',
      repeat($._item),
      '}',
    ),

    // =========================================================================
    // Extern Blocks
    // =========================================================================

    extern_block: $ => seq(
      'extern',
      optional($.string_literal),
      $.extern_body,
    ),

    extern_body: $ => seq(
      '{',
      repeat(choice(
        $.function_signature,
        $.static_declaration,
      )),
      '}',
    ),

    // =========================================================================
    // Actor Definitions (Simplex-specific)
    // =========================================================================

    actor_definition: $ => seq(
      repeat($.attribute),
      optional($.visibility),
      'actor',
      field('name', $._type_identifier),
      optional($.generic_parameters),
      $.actor_body,
    ),

    actor_body: $ => seq(
      '{',
      repeat(choice(
        $.actor_field,
        $.receive_handler,
        $.function_definition,
      )),
      '}',
    ),

    actor_field: $ => seq(
      optional($.visibility),
      choice('let', 'var'),
      field('name', $.identifier),
      ':',
      field('type', $.type),
      optional(seq('=', $._expression)),
      choice(';', ','),
    ),

    receive_handler: $ => seq(
      'receive',
      field('message', $.identifier),
      $.parameters,
      optional(seq('->', field('return_type', $.type))),
      $.block,
    ),

    // =========================================================================
    // AI-Native Features (Simplex-specific)
    // =========================================================================

    specialist_definition: $ => seq(
      repeat($.attribute),
      optional($.visibility),
      'specialist',
      field('name', $._type_identifier),
      $.specialist_body,
    ),

    specialist_body: $ => seq(
      '{',
      repeat(choice(
        $.specialist_config,
        $.function_definition,
      )),
      '}',
    ),

    specialist_config: $ => seq(
      field('key', $.identifier),
      ':',
      field('value', $._expression),
      ',',
    ),

    hive_definition: $ => seq(
      repeat($.attribute),
      optional($.visibility),
      'hive',
      field('name', $._type_identifier),
      $.hive_body,
    ),

    hive_body: $ => seq(
      '{',
      repeat(choice(
        $.hive_config,
        $.function_definition,
      )),
      '}',
    ),

    hive_config: $ => seq(
      field('key', $.identifier),
      ':',
      field('value', $._expression),
      ',',
    ),

    anima_definition: $ => seq(
      repeat($.attribute),
      optional($.visibility),
      'anima',
      field('name', $._type_identifier),
      $.anima_body,
    ),

    anima_body: $ => seq(
      '{',
      repeat(choice(
        $.anima_config,
        $.function_definition,
      )),
      '}',
    ),

    anima_config: $ => seq(
      field('key', $.identifier),
      ':',
      field('value', $._expression),
      ',',
    ),

    // =========================================================================
    // Types
    // =========================================================================

    type: $ => choice(
      $.primitive_type,
      $.reference_type,
      $.pointer_type,
      $.array_type,
      $.slice_type,
      $.tuple_type,
      $.function_type,
      $.generic_type,
      $.path,
      $._type_identifier,
      $.unit_type,
      $.dyn_type,
      $.self_type,
    ),

    primitive_type: $ => choice(
      'i8', 'i16', 'i32', 'i64', 'i128', 'isize',
      'u8', 'u16', 'u32', 'u64', 'u128', 'usize',
      'f32', 'f64',
      'bool',
      'char',
      'str',
      'String',
    ),

    reference_type: $ => seq(
      '&',
      optional($.lifetime),
      optional('mut'),
      $.type,
    ),

    pointer_type: $ => seq(
      '*',
      choice('const', 'mut'),
      $.type,
    ),

    array_type: $ => seq(
      '[',
      field('element', $.type),
      ';',
      field('size', $._expression),
      ']',
    ),

    slice_type: $ => seq(
      '[',
      field('element', $.type),
      ']',
    ),

    tuple_type: $ => seq(
      '(',
      sepBy(',', $.type),
      optional(','),
      ')',
    ),

    function_type: $ => seq(
      optional('unsafe'),
      optional(seq('extern', optional($.string_literal))),
      'fn',
      '(',
      optional(sepBy(',', $.type)),
      optional(','),
      ')',
      optional(seq('->', $.type)),
    ),

    generic_type: $ => prec(1, seq(
      choice($.path, $._type_identifier),
      $.type_arguments,
    )),

    type_arguments: $ => seq(
      '<',
      sepBy(',', choice($.type, $.lifetime)),
      optional(','),
      '>',
    ),

    unit_type: $ => seq('(', ')'),

    dyn_type: $ => seq('dyn', $.type),

    self_type: $ => 'Self',

    lifetime: $ => seq("'", $.identifier),

    // =========================================================================
    // Patterns
    // =========================================================================

    pattern: $ => choice(
      $.identifier_pattern,
      $.wildcard_pattern,
      $.literal_pattern,
      $.tuple_pattern,
      $.struct_pattern,
      $.enum_pattern,
      $.or_pattern,
      $.ref_pattern,
      $.range_pattern,
      $.rest_pattern,
    ),

    identifier_pattern: $ => $.identifier,

    wildcard_pattern: $ => '_',

    literal_pattern: $ => choice(
      $.integer_literal,
      $.float_literal,
      $.string_literal,
      $.char_literal,
      $.boolean_literal,
    ),

    tuple_pattern: $ => seq(
      '(',
      optional(sepBy(',', $.pattern)),
      optional(','),
      ')',
    ),

    struct_pattern: $ => seq(
      $.path,
      '{',
      optional(sepBy(',', choice(
        $.field_pattern,
        $.rest_pattern,
      ))),
      optional(','),
      '}',
    ),

    field_pattern: $ => choice(
      seq(field('name', $.identifier), ':', field('pattern', $.pattern)),
      field('name', $.identifier),
    ),

    enum_pattern: $ => prec(1, seq(
      choice($.path, $._type_identifier),
      optional(choice(
        seq('(', optional(sepBy(',', $.pattern)), optional(','), ')'),
        seq('{', optional(sepBy(',', $.field_pattern)), optional(','), '}'),
      )),
    )),

    or_pattern: $ => prec.left(seq(
      $.pattern,
      '|',
      $.pattern,
    )),

    ref_pattern: $ => prec(2, choice(
      seq('&', 'mut', $.pattern),
      seq('&', $.pattern),
    )),

    range_pattern: $ => seq(
      $.literal_pattern,
      choice('..', '..='),
      $.literal_pattern,
    ),

    rest_pattern: $ => '..',

    // =========================================================================
    // Statements
    // =========================================================================

    block: $ => seq(
      '{',
      repeat($._statement),
      optional($._expression),
      '}',
    ),

    _statement: $ => choice(
      $.let_statement,
      $.expression_statement,
      $.return_statement,
      $.break_statement,
      $.continue_statement,
      $.empty_statement,
    ),

    let_statement: $ => seq(
      choice('let', 'var'),
      optional('mut'),
      field('pattern', $.pattern),
      optional(seq(':', field('type', $.type))),
      optional(seq('=', field('value', $._expression))),
      ';',
    ),

    expression_statement: $ => seq(
      $._expression,
      ';',
    ),

    return_statement: $ => seq(
      'return',
      optional($._expression),
      ';',
    ),

    break_statement: $ => seq(
      'break',
      optional($._expression),
      ';',
    ),

    continue_statement: $ => seq(
      'continue',
      ';',
    ),

    empty_statement: $ => ';',

    // =========================================================================
    // Expressions
    // =========================================================================

    _expression: $ => choice(
      $.identifier,
      $.path,
      $._literal,
      $.unary_expression,
      $.binary_expression,
      $.comparison_expression,
      $.logical_expression,
      $.assignment_expression,
      $.compound_assignment_expression,
      $.call_expression,
      $.method_call_expression,
      $.field_expression,
      $.index_expression,
      $.array_expression,
      $.tuple_expression,
      $.struct_expression,
      $.parenthesized_expression,
      $.block,
      $.if_expression,
      $.match_expression,
      $.for_expression,
      $.while_expression,
      $.loop_expression,
      $.closure_expression,
      $.range_expression,
      $.try_expression,
      $.await_expression,
      $.spawn_expression,
      $.send_expression,
      $.ask_expression,
      $.infer_expression,
      $.cast_expression,
      $.reference_expression,
      $.dereference_expression,
    ),

    unary_expression: $ => prec.right(PREC.unary, choice(
      seq('-', $._expression),
      seq('!', $._expression),
      seq('~', $._expression),
    )),

    binary_expression: $ => choice(
      prec.left(PREC.multiplicative, seq($._expression, '*', $._expression)),
      prec.left(PREC.multiplicative, seq($._expression, '/', $._expression)),
      prec.left(PREC.multiplicative, seq($._expression, '%', $._expression)),
      prec.left(PREC.additive, seq($._expression, '+', $._expression)),
      prec.left(PREC.additive, seq($._expression, '-', $._expression)),
      prec.left(PREC.shift, seq($._expression, '<<', $._expression)),
      prec.left(PREC.shift, seq($._expression, '>>', $._expression)),
      prec.left(PREC.bitwise_and, seq($._expression, '&', $._expression)),
      prec.left(PREC.bitwise_xor, seq($._expression, '^', $._expression)),
      prec.left(PREC.bitwise_or, seq($._expression, '|', $._expression)),
    ),

    comparison_expression: $ => prec.left(PREC.compare, seq(
      $._expression,
      choice('==', '!=', '<', '>', '<=', '>='),
      $._expression,
    )),

    logical_expression: $ => choice(
      prec.left(PREC.and, seq($._expression, '&&', $._expression)),
      prec.left(PREC.or, seq($._expression, '||', $._expression)),
    ),

    assignment_expression: $ => prec.right(PREC.assign, seq(
      field('left', $._expression),
      '=',
      field('right', $._expression),
    )),

    compound_assignment_expression: $ => prec.right(PREC.assign, seq(
      field('left', $._expression),
      choice('+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '<<=', '>>='),
      field('right', $._expression),
    )),

    call_expression: $ => prec(PREC.call, seq(
      field('function', $._expression),
      $.arguments,
    )),

    generic_function: $ => prec(PREC.call, seq(
      field('function', $.path),
      $.type_arguments,
      $.arguments,
    )),

    arguments: $ => seq(
      '(',
      optional(sepBy(',', $._expression)),
      optional(','),
      ')',
    ),

    method_call_expression: $ => prec(PREC.field + 1, seq(
      field('receiver', $._expression),
      '.',
      field('method', $.identifier),
      optional($.type_arguments),
      $.arguments,
    )),

    field_expression: $ => prec(PREC.field, seq(
      field('receiver', $._expression),
      '.',
      field('field', choice($.identifier, $.integer_literal)),
    )),

    index_expression: $ => prec(PREC.call, seq(
      field('value', $._expression),
      '[',
      field('index', $._expression),
      ']',
    )),

    array_expression: $ => seq(
      '[',
      optional(choice(
        seq($._expression, ';', $._expression),
        seq(sepBy(',', $._expression), optional(',')),
      )),
      ']',
    ),

    tuple_expression: $ => seq(
      '(',
      seq($._expression, ','),
      optional(seq(sepBy(',', $._expression), optional(','))),
      ')',
    ),

    struct_expression: $ => seq(
      field('name', choice($.path, $._type_identifier)),
      '{',
      optional(sepBy(',', $.struct_expression_field)),
      optional(','),
      '}',
    ),

    struct_expression_field: $ => choice(
      seq(field('name', $.identifier), ':', field('value', $._expression)),
      field('name', $.identifier),
      seq('..', $._expression),
    ),

    parenthesized_expression: $ => seq(
      '(',
      $._expression,
      ')',
    ),

    if_expression: $ => seq(
      'if',
      choice(
        seq('let', $.pattern, '=', field('condition', $._expression)),
        field('condition', $._expression),
      ),
      field('consequence', $.block),
      optional(seq(
        'else',
        field('alternative', choice($.block, $.if_expression)),
      )),
    ),

    match_expression: $ => seq(
      'match',
      field('value', $._expression),
      '{',
      optional(sepBy(',', $.match_arm)),
      optional(','),
      '}',
    ),

    match_arm: $ => prec(1, seq(
      field('pattern', $.pattern),
      optional(seq('if', field('guard', $._expression))),
      '=>',
      field('body', choice($.block, $._expression)),
    )),

    for_expression: $ => seq(
      'for',
      field('pattern', $.pattern),
      'in',
      field('iterable', $._expression),
      field('body', $.block),
    ),

    while_expression: $ => seq(
      'while',
      choice(
        seq('let', $.pattern, '=', field('condition', $._expression)),
        field('condition', $._expression),
      ),
      field('body', $.block),
    ),

    loop_expression: $ => seq(
      'loop',
      field('body', $.block),
    ),

    closure_expression: $ => prec.right(seq(
      optional('move'),
      $.closure_parameters,
      optional(seq('->', $.type)),
      choice(
        $.block,
        $._expression,
      ),
    )),

    closure_parameters: $ => seq(
      '|',
      optional(sepBy(',', $.closure_parameter)),
      optional(','),
      '|',
    ),

    closure_parameter: $ => seq(
      optional('mut'),
      field('name', $.identifier),
      optional(seq(':', field('type', $.type))),
    ),

    range_expression: $ => prec.left(PREC.range, choice(
      seq($._expression, '..', $._expression),
      seq($._expression, '..=', $._expression),
      seq($._expression, '..'),
      seq('..', $._expression),
      seq('..=', $._expression),
      '..',
    )),

    try_expression: $ => prec.left(PREC.unary, seq(
      $._expression,
      '?',
    )),

    await_expression: $ => prec(PREC.field, seq(
      $._expression,
      '.',
      'await',
    )),

    // Simplex-specific expressions
    spawn_expression: $ => prec.right(seq(
      'spawn',
      $._expression,
    )),

    send_expression: $ => seq(
      'send',
      '(',
      field('target', $._expression),
      ',',
      field('message', $._expression),
      ')',
    ),

    ask_expression: $ => seq(
      'ask',
      '(',
      field('target', $._expression),
      ',',
      field('message', $._expression),
      ')',
    ),

    infer_expression: $ => seq(
      'infer',
      '(',
      field('prompt', $._expression),
      ')',
    ),

    cast_expression: $ => prec.left(PREC.cast, seq(
      $._expression,
      'as',
      $.type,
    )),

    reference_expression: $ => prec.right(PREC.unary, seq(
      '&',
      optional('mut'),
      $._expression,
    )),

    dereference_expression: $ => prec.right(PREC.unary, seq(
      '*',
      $._expression,
    )),

    // =========================================================================
    // Literals
    // =========================================================================

    _literal: $ => choice(
      $.integer_literal,
      $.float_literal,
      $.string_literal,
      $.char_literal,
      $.boolean_literal,
      $.fstring_literal,
    ),

    integer_literal: $ => token(choice(
      // Decimal
      /[0-9][0-9_]*/,
      // Hexadecimal
      /0[xX][0-9a-fA-F_]+/,
      // Octal
      /0[oO][0-7_]+/,
      // Binary
      /0[bB][01_]+/,
    )),

    float_literal: $ => token(choice(
      /[0-9][0-9_]*\.[0-9][0-9_]*/,
      /[0-9][0-9_]*\.[0-9][0-9_]*[eE][+-]?[0-9_]+/,
      /[0-9][0-9_]*[eE][+-]?[0-9_]+/,
    )),

    string_literal: $ => seq(
      '"',
      repeat(choice(
        $.escape_sequence,
        /[^"\\]+/,
      )),
      '"',
    ),

    fstring_literal: $ => seq(
      'f"',
      repeat(choice(
        $.escape_sequence,
        $.interpolation,
        /[^"\\{]+/,
      )),
      '"',
    ),

    interpolation: $ => seq(
      '{',
      $._expression,
      optional(seq(':', $.format_spec)),
      '}',
    ),

    format_spec: $ => /[^}]+/,

    char_literal: $ => seq(
      "'",
      choice(
        $.escape_sequence,
        /[^'\\]/,
      ),
      "'",
    ),

    escape_sequence: $ => token.immediate(choice(
      /\\[nrt\\'"0]/,
      /\\x[0-9a-fA-F]{2}/,
      /\\u\{[0-9a-fA-F]+\}/,
    )),

    boolean_literal: $ => choice('true', 'false'),

    // =========================================================================
    // Paths
    // =========================================================================

    // Qualified paths require at least one :: separator
    path: $ => prec.left(seq(
      optional(choice('::', $.path_segment)),
      repeat1(seq('::', $.path_segment)),
    )),

    path_segment: $ => prec.left(seq(
      $.identifier,
      optional($.type_arguments),
    )),

    // =========================================================================
    // Identifiers
    // =========================================================================

    identifier: $ => /[a-zA-Z_][a-zA-Z0-9_]*/,

    _type_identifier: $ => alias($.identifier, $.type_identifier),

    _field_identifier: $ => alias($.identifier, $.field_identifier),

    // =========================================================================
    // Comments
    // =========================================================================

    line_comment: $ => token(choice(
      seq('//', /[^\n]*/),
    )),

    block_comment: $ => token(seq(
      '/*',
      /[^*]*\*+([^/*][^*]*\*+)*/,
      '/',
    )),

    doc_comment: $ => token(seq(
      '///',
      /[^\n]*/,
    )),
  },
});

// Helper function: separate by delimiter
function sepBy(delimiter, rule) {
  return optional(sepBy1(delimiter, rule));
}

function sepBy1(delimiter, rule) {
  return seq(rule, repeat(seq(delimiter, rule)));
}
