#include "interp.h"
#include "strings.h"
#include "interp.h"

static void report_unexpected_token(Interp *interp, Token *tok, u32 expected) {
    char expected_str[128], got_str[128];
    fmt_tok_kind(expected_str, sizeof(expected_str), expected);
    fmt_tok_kind(got_str, sizeof(got_str), tok->kind);
    report_error_tok(&interp->er, tok, "Token %s expected (got %s)",
        expected_str, got_str);
}

static b32 expect_tok(Interp *interp, Token *tok, u32 kind) {
    b32 result = TRUE;
    if (tok->kind != kind) {
        report_unexpected_token(interp, tok, kind);
        result = FALSE;
    }
    return result;
}

static b32 parse_end_of_statement(Interp *interp, Token *tok) {
    b32 result = expect_tok(interp, tok, ';');
    if (result) {
        eat_tok(interp->tr);
    }
    return result;
}

static AST *ast_new(Interp *interp, u32 kind) {
    AST *ast = arena_alloc_struct(&interp->arena, AST);
    ast->kind = kind;
    // @TODO more explicit way of taking source location?
    ast->src_loc = peek_tok(interp->tr)->src_loc;
    return ast;
}

static AST *create_ident(Interp *interp, const char *name) {
    AST *ident = ast_new(interp, AST_IDENT);
    ident->ident.name = arena_alloc_str(&interp->arena, name);
    return ident;
}

static AST *create_int_lit(Interp *interp, i64 value) {
    AST *literal = ast_new(interp, AST_LITERAL);
    literal->literal.kind = AST_LITERAL_INT;
    literal->literal.value_int = value;
    return literal;
}

ASTList parse_comma_separated_idents(Interp *interp) {
    ASTList idents = {0};
    Token *tok = peek_tok(interp->tr);
    while (tok->kind == TOKEN_IDENT) {
        AST *ident = create_ident(interp, tok->value_str);
        ast_list_add(&idents, ident);
        tok = peek_next_tok(interp->tr);
        if (tok->kind == ',') {
            tok = peek_next_tok(interp->tr);
        }
    }
    return idents;
}

AST *parse_decl(Interp *interp);
AST *parse_decl_ident(Interp *interp, AST *ident, b32 end_of_statement);
AST *parse_block(Interp *interp);

/* 
1: . () []
2: u+ u- ! ~
3: * / %
4: b+ b-
5: << >>
6: < <= > >=
7: == !=
8: &
9: ^
10: | 
11: &&
12: ||
*/
#define MAX_OPEARTOR_PRECEDENCE 12

AST *parse_expr_precedence(Interp *interp, u32 precedence);
static AST *parse_binary_subxepr(Interp *interp, u32 prec, u32 bin_kind, AST *left) {
    AST *result = 0;
    AST *right = parse_expr_precedence(interp, prec - 1);
    if (right) {
        AST *bin = ast_new(interp, AST_BINARY);
        bin->binary.kind = bin_kind;
        bin->binary.left = left;
        bin->binary.right = right; 
        result = bin;
    } else {
        report_error_tok(&interp->er, peek_tok(interp->tr), "Expected expression");
    }
    return result; 
}

// Parse expression with ascending precedence - lower ones are parsed earlier
// Currently this is made by making 
// @TODO remove recursion for precedence iteration
#define parse_expr(_interp) parse_expr_precedence(_interp, MAX_OPEARTOR_PRECEDENCE)
AST *parse_expr_precedence(Interp *interp, u32 precedence) {
    AST *expr = 0;
    switch (precedence) {
        case 0: {
            Token *tok = peek_tok(interp->tr);
            switch (tok->kind) {
                case TOKEN_IDENT: {
                    eat_tok(interp->tr);
                    AST *ident = create_ident(interp, tok->value_str);
                    expr = ident;
                    tok = peek_tok(interp->tr);
                    // @TODO What if some mangling done to function name???
                    // finder better place for function call
                    if (tok->kind == '(') {
                        eat_tok(interp->tr);
                        AST *function_call = ast_new(interp, AST_FUNC_CALL);
                        function_call->func_call.callable = ident;
                        ASTList arguments = parse_comma_separated_idents(interp);
                        if (expect_tok(interp, peek_tok(interp->tr), ')')) {
                            eat_tok(interp->tr);
                            expr = function_call;
                        }
                    }
                } break;
                case TOKEN_INT: {
                    eat_tok(interp->tr);
                    AST *lit = create_int_lit(interp, tok->value_int);
                    expr = lit;
                } break;
                case TOKEN_REAL: {
                    NOT_IMPLEMENTED;
                } break;
            }
        } break;
        case 1: {
            expr = parse_expr_precedence(interp, precedence - 1);
            Token *tok = peek_tok(interp->tr);
            switch (tok->kind) {
                case '(': {
                    eat_tok(interp->tr);
                    AST *temp = parse_expr(interp);
                    tok = peek_tok(interp->tr);
                    if (expect_tok(interp, tok, ')')) {
                        expr = temp;
                        eat_tok(interp->tr);
                    }
                } break;
                case '.': {
                    NOT_IMPLEMENTED;
                } break;
                case '[': {
                    NOT_IMPLEMENTED;
                } break;
            }
        } break;
        case 2: {
            Token *tok = peek_tok(interp->tr);
            switch (tok->kind) {
                case '+': {
                    eat_tok(interp->tr);
                    AST *unary = ast_new(interp, AST_UNARY);
                    unary->unary.kind = AST_UNARY_PLUS;
                    unary->unary.expr = parse_expr(interp);
                    expr = unary;
                } break;
                case '-': {
                    eat_tok(interp->tr);
                    AST *unary = ast_new(interp, AST_UNARY);
                    unary->unary.kind = AST_UNARY_MINUS;
                    unary->unary.expr = parse_expr(interp);
                    expr = unary;
                } break;
                case '!': {
                    eat_tok(interp->tr);
                    AST *unary = ast_new(interp, AST_UNARY);
                    unary->unary.kind = AST_UNARY_LOGICAL_NOT;
                    unary->unary.expr = parse_expr(interp);
                    expr = unary;
                } break;
                case '~': {
                    eat_tok(interp->tr);
                    AST *unary = ast_new(interp, AST_UNARY);
                    unary->unary.kind = AST_UNARY_NOT;
                    unary->unary.expr = parse_expr(interp);
                    expr = unary;
                } break;
            }
            
            if (!expr) {
                expr = parse_expr_precedence(interp, precedence - 1);
            }
        } break;
        case 3: {
            expr = parse_expr_precedence(interp, precedence - 1);
            if (!expr) {
                goto end;
            }
            Token *tok = peek_tok(interp->tr);
            while (tok->kind == '*' || tok->kind == '/' || tok->kind == '%') {
                u32 bin_kind = 0;
                switch (tok->kind) {
                    case '*': {
                        bin_kind = AST_BINARY_MUL;
                    } break;
                    case '/': {
                        bin_kind = AST_BINARY_DIV;
                    } break;
                    case '%': {
                        bin_kind = AST_BINARY_MOD;
                    } break;
                }
                eat_tok(interp->tr);
                expr = parse_binary_subxepr(interp, precedence, bin_kind, expr);
                tok = peek_tok(interp->tr);
            }
        } break;
        case 4: {
            expr = parse_expr_precedence(interp, precedence - 1);
            if (!expr) {
                goto end;
            }
            Token *tok = peek_tok(interp->tr);
            while (tok->kind == '+' || tok->kind == '-') {
                u32 bin_kind = 0;
                if (tok->kind == '+') {
                    bin_kind = AST_BINARY_ADD;
                } else if (tok->kind == '-') {
                    bin_kind = AST_BINARY_SUB;
                }
                eat_tok(interp->tr);
                expr = parse_binary_subxepr(interp, precedence, bin_kind, expr);
                tok = peek_tok(interp->tr);
            }
        } break;
        case 5: {
            expr = parse_expr_precedence(interp, precedence - 1);
            if (!expr) {
                goto end;
            }
            Token *tok = peek_tok(interp->tr);
            while (tok->kind == TOKEN_LSHIFT || tok->kind == TOKEN_RSHIFT) {
                u32 bin_kind = 0;
                if (tok->kind == TOKEN_RSHIFT) {
                    bin_kind = AST_BINARY_RSHIFT;
                } else if (tok->kind == TOKEN_LSHIFT) {
                    bin_kind = AST_BINARY_SUB;
                }
                eat_tok(interp->tr);
                expr = parse_binary_subxepr(interp, precedence, bin_kind, expr);
                tok = peek_tok(interp->tr);
            }
        } break;
        case 6: {
            expr = parse_expr_precedence(interp, precedence - 1);
            if (!expr) {
                goto end;
            }
            Token *tok = peek_tok(interp->tr);
            while (tok->kind == '<' || tok->kind == '>' || tok->kind == TOKEN_LE || tok->kind == TOKEN_GE) {
                u32 bin_kind = 0;
                if (tok->kind == TOKEN_LE) {
                    bin_kind = AST_BINARY_LE;
                } else if (tok->kind == TOKEN_GE) {
                    bin_kind = AST_BINARY_GE;
                } else if (tok->kind == '<') {
                    bin_kind = AST_BINARY_L;
                } else if (tok->kind == '>') {
                    bin_kind = AST_BINARY_G;
                }
                eat_tok(interp->tr);
                expr = parse_binary_subxepr(interp, precedence, bin_kind, expr);
                tok = peek_tok(interp->tr);
            }
        } break;
        case 7: {
            expr = parse_expr_precedence(interp, precedence - 1);
            if (!expr) {
                goto end;
            }
            Token *tok = peek_tok(interp->tr);
            while (tok->kind == TOKEN_EQ || tok->kind == TOKEN_NEQ) {
                u32 bin_kind = 0;
                if (tok->kind == TOKEN_EQ) {
                    bin_kind = AST_BINARY_EQ;
                } else if (tok->kind == TOKEN_NEQ) {
                    bin_kind = AST_BINARY_NEQ;
                }
                eat_tok(interp->tr);
                expr = parse_binary_subxepr(interp, precedence, bin_kind, expr);
                tok = peek_tok(interp->tr);
            }
        } break;
        case 8: {
            expr = parse_expr_precedence(interp, precedence - 1);
            if (!expr) {
                goto end;
            }
            Token *tok = peek_tok(interp->tr);
            while (tok->kind == '&') {
                u32 bin_kind = AST_BINARY_AND;
                eat_tok(interp->tr);
                expr = parse_binary_subxepr(interp, precedence, bin_kind, expr);
                tok = peek_tok(interp->tr);
            }
        } break;
        case 9: {
            expr = parse_expr_precedence(interp, precedence - 1);
            if (!expr) {
                goto end;
            }
            Token *tok = peek_tok(interp->tr);
            while (tok->kind == '&') {
                u32 bin_kind = AST_BINARY_XOR;
                eat_tok(interp->tr);
                expr = parse_binary_subxepr(interp, precedence, bin_kind, expr);
                tok = peek_tok(interp->tr);
            }
        } break;
        case 10: {
            expr = parse_expr_precedence(interp, precedence - 1);
            if (!expr) {
                goto end;
            }
            Token *tok = peek_tok(interp->tr);
            while (tok->kind == '&') {
                u32 bin_kind = AST_BINARY_OR;
                eat_tok(interp->tr);
                expr = parse_binary_subxepr(interp, precedence, bin_kind, expr);
                tok = peek_tok(interp->tr);
            }
        } break;
        case 11: {
            expr = parse_expr_precedence(interp, precedence - 1);
            if (!expr) {
                goto end;
            }
            Token *tok = peek_tok(interp->tr);
            while (tok->kind == TOKEN_LOGICAL_AND) {
                u32 bin_kind = AST_BINARY_LOGICAL_AND;
                eat_tok(interp->tr);
                expr = parse_binary_subxepr(interp, precedence, bin_kind, expr);
                tok = peek_tok(interp->tr);
            }
        } break;
        case 12: {
            expr = parse_expr_precedence(interp, precedence - 1);
            if (!expr) {
                goto end;
            }
            Token *tok = peek_tok(interp->tr);
            while (tok->kind == TOKEN_LOGICAL_OR) {
                u32 bin_kind = AST_BINARY_LOGICAL_OR;
                eat_tok(interp->tr);
                expr = parse_binary_subxepr(interp, precedence, bin_kind, expr);
                tok = peek_tok(interp->tr);
            }
        } break;
        default: assert(FALSE);
    }
end:
    return expr;
}

static AST *parse_type(Interp *interp) {
    AST *type = 0;
    Token *tok = peek_tok(interp->tr);
    if (tok->kind == TOKEN_KW_INT) {
        type = ast_new(interp, AST_TYPE);
        type->type.kind = AST_TYPE_INT;
        eat_tok(interp->tr);
    } else if (tok->kind == TOKEN_KW_FLOAT) {
        type = ast_new(interp, AST_TYPE);
        type->type.kind = AST_TYPE_FLOAT;
        eat_tok(interp->tr);
    } else {
        
    }
    return type;
}

static u32 assign_token_to_binary_kind(u32 tok) {
    u32 result = 0;
    // @TODO This can be optimized if we structure the ast binary enum 
    // to have kinds in the same order as corresponding token kinds
    switch (tok) {
        case TOKEN_IADD: {
            result = AST_BINARY_ADD;
        } break;
        case TOKEN_ISUB: {
            result = AST_BINARY_SUB;
        } break;
        case TOKEN_IMUL: {
            result = AST_BINARY_MUL;
        } break;
        case TOKEN_IDIV: {
            result = AST_BINARY_DIV;
        } break;
        case TOKEN_IMOD: {
            result = AST_BINARY_MOD;
        } break;
        case TOKEN_IAND: {
            result = AST_BINARY_AND;
        } break;
        case TOKEN_IOR: {
            result = AST_BINARY_OR;
        } break;
        case TOKEN_IXOR: {
            result = AST_BINARY_XOR;
        } break;
        case TOKEN_ILSHIFT: {
            result = AST_BINARY_LSHIFT;
        } break;
        case TOKEN_IRSHIFT: {
            result = AST_BINARY_RSHIFT;
        } break;
    }
    return result;
}

AST *parse_assign_ident(Interp *interp, AST *ident) {
    AST *assign = 0;
    Token *tok = peek_tok(interp->tr);
    if (!is_token_assign(tok->kind)) {
        report_error_tok(&interp->er, tok, "Expected assign operator");
        return 0;    
    }
    
    // All incorrect tokens should be handled before
    eat_tok(interp->tr);
    // @TODO make this more readable...
    if (tok->kind == '=') {
        AST *expr = parse_expr(interp);
        
        assign = ast_new(interp, AST_ASSIGN);
        assign->assign.ident = ident;
        assign->assign.expr = expr;
    } else {
        u32 binary_kind = assign_token_to_binary_kind(tok->kind);
        assert(binary_kind);
        AST *binary = ast_new(interp, AST_BINARY);
        binary->binary.kind = binary_kind;
        binary->binary.left = ident;
        binary->binary.right = parse_expr(interp);
        assign = ast_new(interp, AST_ASSIGN);
        assign->assign.ident = ident;
        assign->assign.expr = binary;
    }
    
    tok = peek_tok(interp->tr);
    if (!parse_end_of_statement(interp, tok)) {
        assign = 0;
    }
    return assign;
}

AST *parse_if_compound(Interp *interp) {
    Token *tok = peek_tok(interp->tr);
    if (tok->kind != TOKEN_KW_IF) {
        return 0;
    }
    
    tok = peek_next_tok(interp->tr);
    AST *expr = parse_expr(interp);
    if (!expr) {
        return 0;
    }
    
    AST *if_st = ast_new(interp, AST_IF);
    if_st->if_st.cond = expr;
    tok = peek_tok(interp->tr);
    if (expect_tok(interp, tok, '{')) {
        AST *block = parse_block(interp);
        if (!block) {
            return 0;
        }
        if_st->if_st.block = block;
        tok = peek_tok(interp->tr);
        if (tok->kind == TOKEN_KW_ELSE) {
            tok = peek_next_tok(interp->tr);
            if (tok->kind == TOKEN_KW_IF) {
                AST *else_if_st = parse_if_compound(interp);
                if_st->if_st.else_block = else_if_st;
            } else if (tok->kind == '{') {
                AST *else_block = parse_block(interp);
                if_st->if_st.else_block = else_block;
            }
        }
    }
    return if_st;
}

AST *parse_statement(Interp *interp) {
    AST *statement = 0;
    Token *tok = peek_tok(interp->tr);
    // Common path for assign and decl
    // @TODO maybe unite assign and declaration
    AST *ident = 0;
    if (tok->kind == TOKEN_IDENT) {
        ident = create_ident(interp, tok->value_str);
        tok = peek_next_tok(interp->tr);
    } 
    
    if (is_token_assign(tok->kind)) {
        statement = parse_assign_ident(interp, ident);
    } else if (tok->kind == TOKEN_KW_RETURN) {
        tok = peek_next_tok(interp->tr);
        ASTList return_vars = {0};
        while (tok->kind != ';') {
            AST *expr = parse_expr(interp);
            if (!expr || is_error_reported(&interp->er)) {
                break;
            }
            ast_list_add(&return_vars, expr);
            
            tok = peek_tok(interp->tr);
            if (tok->kind == ',') {
                tok = peek_next_tok(interp->tr);
            }
        }
        
        if (parse_end_of_statement(interp, tok)) {
            statement = ast_new(interp, AST_RETURN);
            statement->return_st.vars = return_vars;
        }
    } else if (tok->kind == TOKEN_KW_IF) {
        AST *if_st = parse_if_compound(interp);
        statement = if_st;
    } else if (tok->kind == TOKEN_KW_WHILE) {
        eat_tok(interp->tr);
        AST *condition = parse_expr(interp);
        if (condition) {
            AST *block = parse_block(interp);
            if (block) {
                AST *while_st = ast_new(interp, AST_WHILE);
                while_st->while_st.cond = condition;
                while_st->while_st.block = block;
                
                statement = while_st;
            }
        }
    } else if (tok->kind == TOKEN_KW_PRINT) {
        eat_tok(interp->tr);
        AST *expr = parse_expr(interp);
        ASTList exprs = {0};
        ast_list_add(&exprs, expr);
        tok = peek_tok(interp->tr);
        if (parse_end_of_statement(interp, tok)) {
            AST *print_st = ast_new(interp, AST_PRINT);
            print_st->print_st.arguments = exprs;
            statement = print_st;
        }
    } else {
        statement = parse_decl_ident(interp, ident, TRUE);
    }
    
    return statement;
}

AST *parse_block(Interp *interp) {
    AST *block = 0;
    Token *tok = peek_tok(interp->tr);
    if (expect_tok(interp, tok, '{')) {
        tok = peek_next_tok(interp->tr);     
    } else {
        return 0;
    }
    
    ASTList statements = {0};
    while (peek_tok(interp->tr)->kind != '}') {
        AST *statement = parse_statement(interp);
        if (!statement) {
            break;
        }
        
        if (is_error_reported(&interp->er)) {
            break;
        }
        ast_list_add(&statements, statement);
    }
    block = ast_new(interp, AST_BLOCK);
    block->block.statements = statements;
    
    tok = peek_tok(interp->tr);
    if (expect_tok(interp, tok, '}')) {
        tok = peek_next_tok(interp->tr);     
    } else {
        return 0;
    }
    return block;
}

AST *parse_function_signature(Interp *interp) {
    Token *tok = peek_tok(interp->tr);
    if (!expect_tok(interp, tok, '(')) {
        return 0;
    }
    tok = peek_next_tok(interp->tr);
    // Parse arguments
    ASTList args = {0};
    while (tok->kind != ')') {
        if (expect_tok(interp, tok, TOKEN_IDENT)) {
            AST *ident = create_ident(interp, tok->value_str);
            eat_tok(interp->tr);
            AST *decl = parse_decl_ident(interp, ident, FALSE);
            ast_list_add(&args, ident);
            tok = peek_tok(interp->tr);
        } else {
            break;
        }
    }
    if (is_error_reported(&interp->er)) {
        return 0;
    }
    
    AST *sign = ast_new(interp, AST_FUNC_SIGNATURE);
    sign->func_sign.arguments = args;
    
    tok = peek_next_tok(interp->tr);
    if (tok->kind == TOKEN_ARROW) {
        tok = peek_next_tok(interp->tr);
        
        ASTList return_types = {0};
        Token *tok = peek_tok(interp->tr);
        for (;;) {
            AST *type = parse_type(interp);
            if (type) {
                ast_list_add(&return_types, type);
                tok = peek_tok(interp->tr);
                if (tok->kind == ',') {
                    tok = peek_next_tok(interp->tr);
                }
            } else {
                break;
            }
        }
        
        sign->func_sign.return_types = return_types;
    }
    return sign;
}

AST *parse_decl_ident(Interp *interp, AST *ident, b32 end_of_statement) {
    AST *decl = 0;
    Token *tok = peek_tok(interp->tr);
    switch (tok->kind) {
        case TOKEN_AUTO_DECL: {
            tok = peek_next_tok(interp->tr);
            if (tok->kind == '(') {
                AST *func_sign = parse_function_signature(interp);
                if (func_sign) {
                    AST *func_block = parse_block(interp);
                    if (func_block) {
                        decl = ast_new(interp, AST_FUNC_DECL);
                        decl->func_decl.name = ident;
                        decl->func_decl.sign = func_sign;
                        decl->func_decl.block = func_block;
                    }
                }
            } else {
                AST *expr = parse_expr(interp);
                if (expr) {
                    decl = ast_new(interp, AST_DECL);
                    decl->decl.ident = ident;
                    decl->decl.expr = expr;
                    decl->decl.is_immutable = FALSE;
                    
                    tok = peek_tok(interp->tr);
                    if (end_of_statement && !parse_end_of_statement(interp, tok)) {
                        decl = 0;
                    }
                }
            }
        } break;
        case ':': {
            eat_tok(interp->tr);
            AST *type = parse_type(interp);
            assert(type);
            decl = ast_new(interp, AST_DECL);
            decl->decl.ident = ident; 
            decl->decl.type = type;  
            tok = peek_tok(interp->tr);
            if (tok->kind == '=') {
                tok = peek_next_tok(interp->tr);
                AST *expr = parse_expr(interp);
                decl->decl.expr = expr;
            } 
            
            tok = peek_tok(interp->tr);
            if (end_of_statement && !parse_end_of_statement(interp, tok)) {
                decl = 0;
            }
        } break;
        default: {
            report_error_tok(&interp->er, tok, "Expected := or :: or : in declaration");
        } break;
    }
    
    return decl;
}

AST *parse_decl(Interp *interp) {
    AST *decl = 0;
    Token *tok = peek_tok(interp->tr);
    if (expect_tok(interp, tok, TOKEN_IDENT)) {
        AST *ident = create_ident(interp, tok->value_str);
        eat_tok(interp->tr);
        decl = parse_decl_ident(interp, ident, TRUE);
    }
    
    return decl;
}

AST *parse_toplevel_item(Interp *interp) {
    Token *tok = peek_tok(interp->tr);
    if (tok->kind == TOKEN_EOS) {
        return 0;
    }
    
    AST *item = parse_decl(interp);
    return item;
}

Interp *create_interp(const char *filename, const char *out_filename) {
    Interp *interp = arena_bootstrap(Interp, arena);
    interp->out_filename = out_filename;
    interp->in_file_id = fs_open_file(filename, FILE_MODE_READ);
    init_in_streamf(&interp->file_in_st, fs_get_handle(interp->in_file_id), 
        arena_alloc(&interp->arena, IN_STREAM_DEFAULT_BUFFER_SIZE), IN_STREAM_DEFAULT_BUFFER_SIZE,
        IN_STREAM_DEFAULT_THRESHLOD, FALSE);
    interp->tr = create_tokenizer(&interp->file_in_st, interp->in_file_id);
    interp->bytecode_builder = create_bytecode_builder(&interp->er);
    return interp;
}

void do_interp(Interp *interp) {
    for (;;) {
        AST *toplevel = parse_toplevel_item(interp);
        if (!toplevel || is_error_reported(&interp->er)) {
            break;
        }
        
        bytecode_builder_proccess_toplevel(interp->bytecode_builder, toplevel);
        // fmt_ast_tree_recursive(get_stdout_stream(), toplevel, 0);
        // out_stream_flush(get_stdout_stream());
    }
    
    if (!is_error_reported(&interp->er)) {
        FileID out_file = fs_open_file(interp->out_filename, FILE_MODE_WRITE);
        bytecode_builder_emit_code(interp->bytecode_builder, fs_get_handle(out_file));
        fs_close_file(out_file);
    }
}

void destroy_interp(Interp *interp) {
    destroy_bytecode_builder(interp->bytecode_builder);
    destroy_tokenizer(interp->tr);
    fs_close_file(interp->in_file_id);
    arena_clear(&interp->arena);
}
