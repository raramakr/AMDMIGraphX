import cppcheck, itertools
from cppcheckdata import simpleMatch, match #patterns, bind_split


def skipTokenMatches(tokens, skip=None):
    for tok in tokens:
        if match(tok, skip):
            continue
        yield tok


def isTokensEqual(xtokens, ytokens, skip=None):
    for x, y in itertools.zip_longest(skipTokenMatches(xtokens, skip),
                                      skipTokenMatches(ytokens, skip)):
        if not x:
            return False
        if not y:
            return False
        if x.str != y.str:
            return False
    return True


def getInnerLink(token):
    if not token:
        return []
    if not token.link:
        return []
    return token.next.forward(token.link)


def getVariableDecl(var):
    if not var:
        return []
    end = var.typeEndToken
    if end:
        end = end.next
    return var.typeStartToken.forward(end)

def isFunctionCall(token):
    if not token:
        return False
    if not token.isName:
        return False
    if not token.next:
        return False
    if not token.next.str == '(':
        return False
    return True

# def isplit(source, sep=' '):
#     sepsize = len(sep)
#     start = 0
#     while True:
#         idx = source.find(sep, start)
#         if idx == -1:
#             yield source[start:]
#             return
#         yield source[start:idx]
#         start = idx + sepsize

# def match_atom(token, p):
#     if not token:
#         return None
#     if not p:
#         return None
#     if token.str == p:
#         return token
#     if p in ['!', '|', '||', '%', '!=', '*']:
#         return None
#     if p in patterns:
#         return patterns[p](token)
#     if '|' in p:
#         for x in isplit(p, '|'):
#             t = match_atom(token, x)
#             if t:
#                 return t
#     elif p.startswith('!!'):
#         t = match_atom(token, p[2:])
#         if not t:
#             return token
#     elif p.startswith('**'):
#         a = p[2:]
#         t = token
#         while t:
#             if match_atom(t, a):
#                 return t
#             if t.link and t.str in ['(', '[', '<', '{']:
#                 t = t.link
#             t = t.next
#     return None

# class MatchResult:
#     def __init__(self, matches, bindings=None, keys=None):
#         self.__dict__.update(bindings or {})
#         self._matches = matches
#         self._keys = keys or []

#     def __bool__(self):
#         return self._matches

#     def __nonzero__(self):
#         return self._matches

#     def __getattr__(self, k):
#         if not self._matches:
#             return None
#         if k in self._keys:
#             return None
#         else:
#             raise AttributeError

# def match(token, pattern):
#     if not pattern:
#         return MatchResult(False)
#     end = None
#     bindings = {}
#     words = (bind_split(word) for word in isplit(pattern))
#     for p, b in words:
#         t = match_atom(token, p)
#         if b:
#             bindings[b] = token
#         if not t:
#             return MatchResult(False)
#         end = t
#         token = t.next
#     bindings['end'] = end
#     return MatchResult(True, bindings=bindings)

@cppcheck.checker
def AvoidBranchingStatementAsLastInLoop(cfg, data):
    for token in cfg.tokenlist:
        if not token.str in ['for', 'while']:
            continue
        end = match(token, "for|while (*) {*}").end
        if not end:
            continue
        stmt = end.tokAt(-2)
        if not match(stmt, "%any% ; }"):
            continue
        if not match(stmt, "break|continue"):
            stmt = stmt.astTop()
        if match(stmt, "break|continue|return"):
            cppcheck.reportError(
                stmt, "style",
                "Branching statement as the last statement inside a loop is very confusing."
            )


# @cppcheck.checker
# def CollapsibleIfStatements(cfg, data):
#     for token in cfg.tokenlist:
#         if not match(token, "if (*) { if (*) {*} }"):
#             continue
#         cppcheck.reportError(token, "style", "These two if statements can be collapsed into one.")


@cppcheck.checker
def ConditionalAssert(cfg, data):
    for token in cfg.tokenlist:
        # if token.str != 'if':
            # continue
        if not match(token, "if (*) { assert (*) ; }"):
            continue
        cppcheck.reportError(token, "style",
                             "The if condition should be included in assert.")


@cppcheck.checker
def EmptyCatchStatement(cfg, data):
    for token in cfg.tokenlist:
        # if token.str != 'catch':
            # continue
        if not match(token, "catch (*) { }"):
            continue
        cppcheck.reportError(token, "style", "An empty catch statement.")


@cppcheck.checker
def EmptyDoWhileStatement(cfg, data):
    for token in cfg.tokenlist:
        # if token.str != 'do':
            # continue
        if not simpleMatch(token, "do { } while ("):
            continue
        cppcheck.reportError(token, "style", "Empty do-while.")


@cppcheck.checker
def EmptyElseBlock(cfg, data):
    for token in cfg.tokenlist:
        # if token.str != 'else':
            # continue
        if not simpleMatch(token, "else { }"):
            continue
        cppcheck.reportError(token, "style",
                             "Empty else statement can be safely removed.")


@cppcheck.checker
def EmptyForStatement(cfg, data):
    for token in cfg.tokenlist:
        # if token.str != 'for':
            # continue
        if not match(token, "for (*) { }"):
            continue
        cppcheck.reportError(token, "style", "Empty for statement.")


@cppcheck.checker
def EmptyIfStatement(cfg, data):
    for token in cfg.tokenlist:
        # if token.str != 'if':
            # continue
        if not match(token, "if (*) { }"):
            continue
        cppcheck.reportError(token, "style", "Empty if statement.")


@cppcheck.checker
def EmptySwitchStatement(cfg, data):
    for token in cfg.tokenlist:
        # if token.str != 'switch':
            # continue
        if not match(token, "switch (*) { }"):
            continue
        cppcheck.reportError(token, "style", "Empty switch statement.")


@cppcheck.checker
def EmptyWhileStatement(cfg, data):
    for token in cfg.tokenlist:
        # if token.str != 'while':
            # continue
        if not match(token, "while (*) { }"):
            continue
        cppcheck.reportError(token, "style", "Empty while statement.")


@cppcheck.checker
def ForLoopShouldBeWhileLoop(cfg, data):
    for token in cfg.tokenlist:
        # if token.str != 'for':
            # continue
        if not match(token, "for ( ; !!;"):
            continue
        # Skip empty for loops
        if match(token, "for (*) { }"):
            continue
        end = token.next.link
        if not match(end.tokAt(-1), "; )"):
            continue
        cppcheck.reportError(token, "style",
                             "For loop should be written as a while loop.")


@cppcheck.checker
def GotoStatement(cfg, data):
    for token in cfg.tokenlist:
        if token.str != 'goto':
            continue
        cppcheck.reportError(token, "style", "Goto considered harmful.")


# @cppcheck.checker
# def InvertedLogic(cfg, data):
#     for token in cfg.tokenlist:
#         cond = None
#         if match(token, "if (*) {*} else { !!if"):
#             cond = token.next.astOperand2
#         elif match(token, "?"):
#             cond = token.astOperand1
#         if not match(cond, "!|!="):
#             continue
#         cppcheck.reportError(cond, "style", "It is cleaner to invert the logic.")


@cppcheck.checker
def LambdaAttribute(cfg, data):
    for token in cfg.tokenlist:
        # if token.str != ']':
            # continue
        if not match(token, "] __device__|__host__ {|("):
            continue
        cppcheck.reportError(
            token, "style",
            "Attributes to lambdas must come after parameter list.")


@cppcheck.checker
def MultipleUnaryOperator(cfg, data):
    for token in cfg.tokenlist:
        if not token.isUnaryOp(token.str):
            continue
        if not token.str in ['+', '-', '~', '!']:
            continue
        if not match(token.astOperand1, "+|-|~|!"):
            continue
        if not token.astOperand1.isUnaryOp(token.astOperand1.str):
            continue
        cppcheck.reportError(token, "style",
                             "Muliple unary operators used together.")


@cppcheck.checker
def MutableVariable(cfg, data):
    for token in cfg.tokenlist:
        # if token.str != 'mutable':
            # continue
        if not match(token, "mutable %var%"):
            continue
        cppcheck.reportError(token, "style",
                             "Do not create mutable variables.")


@cppcheck.checker
def NestedBlocks(cfg, data):
    for token in cfg.tokenlist:
        if not token.str in ['if', 'while', 'for', 'switch']:
            continue
        block = match(token, "if|while|for|switch (*) { {*}@block }").block
        if not block:
            block = match(token, "; { {*}@block break ; }").block
        if not block:
            continue
        cppcheck.reportError(block, "style", "Block directly inside block.")


@cppcheck.checker
def RedundantCast(cfg, data):
    for token in cfg.tokenlist:
        if not token.variable:
            continue
        m = match(token,
                  "%var%@decl ; %var%@assign = static_cast <*>@cast (*) ;")
        if not m:
            continue
        if m.decl.varId != m.assign.varId:
            continue
        if not simpleMatch(token.previous, "auto"):
            if not isTokensEqual(getVariableDecl(m.decl.variable),
                                 getInnerLink(m.cast),
                                 skip='const|volatile|&|&&|*'):
                continue
        cppcheck.reportError(token, "style", "Static cast is redundant.")


@cppcheck.checker
def RedundantConditionalOperator(cfg, data):
    for token in cfg.tokenlist:
        # if token.str != '?':
            # continue
        if not match(token, "? true|false : true|false"):
            continue
        cppcheck.reportError(token, "style",
                             "Conditional operator is redundant.")


@cppcheck.checker
def RedundantIfStatement(cfg, data):
    for token in cfg.tokenlist:
        # if token.str != 'if':
            # continue
        if not match(
                token,
                "if (*) { return true|false ; } else { return true|false ; }"):
            continue
        cppcheck.reportError(token, "style", "The if statement is redundant.")


@cppcheck.checker
def RedundantLocalVariable(cfg, data):
    for token in cfg.tokenlist:
        if not token.variable:
            continue
        m = match(token,
                  "%var%@decl ; %var%@assign = **; return %var%@returned ;")
        if not m:
            continue
        if m.decl.varId != m.assign.varId:
            continue
        if m.decl.varId != m.returned.varId:
            continue
        cppcheck.reportError(
            m.returned, "style",
            "Variable is returned immediately after its declaration, can be simplified to just return expression."
        )


# @cppcheck.checker
# def UnnecessaryElseStatement(cfg, data):
#     for token in cfg.tokenlist:
#         m = match(token, "if (*) {*}@block else@else_statement {")
#         if not m:
#             continue
#         stmt = m.block.link.tokAt(-2)
#         if not match(stmt, "%any% ; }"):
#             continue
#         if not match(stmt, "break|continue"):
#             stmt = stmt.astTop()
#         if not match(stmt, "break|continue|return|throw"):
#             continue
#         cppcheck.reportError(m.else_statement, "style", "Else statement is not necessary.")


@cppcheck.checker
def UnnecessaryEmptyCondition(cfg, data):
    for token in cfg.tokenlist:
        # if token.str != 'if':
            # continue
        m = match(token, "if (*)@if_cond { for (*)@for_cond {*} }")
        if not m:
            continue
        cond = m.if_cond.astOperand2
        if match(cond, "!"):
            cond = cond.astOperand1
        if not match(cond.tokAt(-2), ". empty ("):
            continue
        container = cond.tokAt(-2).astOperand1
        if not container.varId:
            continue
        if not match(m.for_cond.astOperand2, ":"):
            continue
        container_iter = m.for_cond.astOperand2.astOperand2
        if container_iter.varId != container.varId:
            continue
        cppcheck.reportError(
            container, "style",
            "Unnecessary check for empty before for range loop.")


@cppcheck.checker
def UseDeviceLaunch(cfg, data):
    for token in cfg.tokenlist:
        if not simpleMatch(token, "hipLaunchKernelGGL ("):
            continue
        cppcheck.reportError(token, "style", "Use device::launch instead.")


@cppcheck.checker
def UseManagePointer(cfg, data):
    functions = {"fclose", "free", "hipFree", "hipHostFree", "hipFreeArray", "hipMemFree", "hipStreamDestroy", "hipEventDestroy", "hipArrayDestroy", "hipCtxDestroy", "hipDestroyTextureObject", "hipDestroySurfaceObject", "miirDestroyHandle"}
    for token in cfg.tokenlist:
        if not isFunctionCall(token):
            continue
        if not token.str in functions:
            continue
        cppcheck.reportError(token, "style",
                             "Use manage pointer for resource management.")


@cppcheck.checker
def UseSmartPointer(cfg, data):
    for token in cfg.tokenlist:
        # if token.str != 'new':
            # continue
        if not match(token, "new %name%"):
            continue
        cppcheck.reportError(token, "style",
                             "Use manage pointer for resource management.")


@cppcheck.checker
def useStlAlgorithms(cfg, data):
    copy_functions = {"memcpy", "strcpy", "strncpy", "strcat", "strncat"}
    for token in cfg.tokenlist:
        if not isFunctionCall(token):
            continue
        if token.str in copy_functions:
            cppcheck.reportError(token, "style", "Use std::copy instead.")
        elif token.str == 'memset':
            cppcheck.reportError(token, "style", "Use std::fill instead.")
        elif token.str == 'memcmp':
            cppcheck.reportError(token, "style",
                                 "Use std::equal_range instead.")
        elif token.str == 'memchr':
            cppcheck.reportError(token, "style", "Use std::find instead.")
