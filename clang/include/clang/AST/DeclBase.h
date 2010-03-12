//===-- DeclBase.h - Base Classes for representing declarations -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the Decl and DeclContext interfaces.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_DECLBASE_H
#define LLVM_CLANG_AST_DECLBASE_H

#include "clang/AST/Attr.h"
#include "clang/AST/Type.h"
// FIXME: Layering violation
#include "clang/Parse/AccessSpecifier.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/ADT/PointerUnion.h"

namespace clang {
class DeclContext;
class TranslationUnitDecl;
class NamespaceDecl;
class UsingDirectiveDecl;
class NamedDecl;
class FunctionDecl;
class CXXRecordDecl;
class EnumDecl;
class ObjCMethodDecl;
class ObjCContainerDecl;
class ObjCInterfaceDecl;
class ObjCCategoryDecl;
class ObjCProtocolDecl;
class ObjCImplementationDecl;
class ObjCCategoryImplDecl;
class ObjCImplDecl;
class LinkageSpecDecl;
class BlockDecl;
class DeclarationName;
class CompoundStmt;
}

namespace llvm {
// DeclContext* is only 4-byte aligned on 32-bit systems.
template<>
  class PointerLikeTypeTraits<clang::DeclContext*> {
  typedef clang::DeclContext* PT;
public:
  static inline void *getAsVoidPointer(PT P) { return P; }
  static inline PT getFromVoidPointer(void *P) {
    return static_cast<PT>(P);
  }
  enum { NumLowBitsAvailable = 2 };
};
}

namespace clang {

/// Decl - This represents one declaration (or definition), e.g. a variable,
/// typedef, function, struct, etc.
///
class Decl {
public:
  /// \brief Lists the kind of concrete classes of Decl.
  enum Kind {
#define DECL(Derived, Base) Derived,
#define DECL_RANGE(CommonBase, Start, End) \
    CommonBase##First = Start, CommonBase##Last = End,
#define LAST_DECL_RANGE(CommonBase, Start, End) \
    CommonBase##First = Start, CommonBase##Last = End
#include "clang/AST/DeclNodes.def"
  };

  /// IdentifierNamespace - According to C99 6.2.3, there are four
  /// namespaces, labels, tags, members and ordinary
  /// identifiers. These are meant as bitmasks, so that searches in
  /// C++ can look into the "tag" namespace during ordinary lookup. We
  /// use additional namespaces for Objective-C entities.  We also put
  /// C++ friend declarations (of previously-undeclared entities) in
  /// shadow namespaces, and 'using' declarations (as opposed to their
  /// implicit shadow declarations) can be found in their own
  /// namespace.
  enum IdentifierNamespace {
    IDNS_Label = 0x1,
    IDNS_Tag = 0x2,
    IDNS_Member = 0x4,
    IDNS_Ordinary = 0x8,
    IDNS_ObjCProtocol = 0x10,
    IDNS_ObjCImplementation = 0x20,
    IDNS_ObjCCategoryName = 0x40,
    IDNS_OrdinaryFriend = 0x80,
    IDNS_TagFriend = 0x100,
    IDNS_Using = 0x200
  };

  /// ObjCDeclQualifier - Qualifier used on types in method declarations
  /// for remote messaging. They are meant for the arguments though and
  /// applied to the Decls (ObjCMethodDecl and ParmVarDecl).
  enum ObjCDeclQualifier {
    OBJC_TQ_None = 0x0,
    OBJC_TQ_In = 0x1,
    OBJC_TQ_Inout = 0x2,
    OBJC_TQ_Out = 0x4,
    OBJC_TQ_Bycopy = 0x8,
    OBJC_TQ_Byref = 0x10,
    OBJC_TQ_Oneway = 0x20
  };

private:
  /// NextDeclInContext - The next declaration within the same lexical
  /// DeclContext. These pointers form the linked list that is
  /// traversed via DeclContext's decls_begin()/decls_end().
  Decl *NextDeclInContext;

  friend class DeclContext;

  struct MultipleDC {
    DeclContext *SemanticDC;
    DeclContext *LexicalDC;
  };


  /// DeclCtx - Holds either a DeclContext* or a MultipleDC*.
  /// For declarations that don't contain C++ scope specifiers, it contains
  /// the DeclContext where the Decl was declared.
  /// For declarations with C++ scope specifiers, it contains a MultipleDC*
  /// with the context where it semantically belongs (SemanticDC) and the
  /// context where it was lexically declared (LexicalDC).
  /// e.g.:
  ///
  ///   namespace A {
  ///      void f(); // SemanticDC == LexicalDC == 'namespace A'
  ///   }
  ///   void A::f(); // SemanticDC == namespace 'A'
  ///                // LexicalDC == global namespace
  llvm::PointerUnion<DeclContext*, MultipleDC*> DeclCtx;

  inline bool isInSemaDC() const    { return DeclCtx.is<DeclContext*>(); }
  inline bool isOutOfSemaDC() const { return DeclCtx.is<MultipleDC*>(); }
  inline MultipleDC *getMultipleDC() const {
    return DeclCtx.get<MultipleDC*>();
  }
  inline DeclContext *getSemanticDC() const {
    return DeclCtx.get<DeclContext*>();
  }

  /// Loc - The location that this decl.
  SourceLocation Loc;

  /// DeclKind - This indicates which class this is.
  Kind DeclKind   :  8;

  /// InvalidDecl - This indicates a semantic error occurred.
  unsigned int InvalidDecl :  1;

  /// HasAttrs - This indicates whether the decl has attributes or not.
  unsigned int HasAttrs : 1;

  /// Implicit - Whether this declaration was implicitly generated by
  /// the implementation rather than explicitly written by the user.
  bool Implicit : 1;

  /// \brief Whether this declaration was "used", meaning that a definition is
  /// required.
  bool Used : 1;

protected:
  /// Access - Used by C++ decls for the access specifier.
  // NOTE: VC++ treats enums as signed, avoid using the AccessSpecifier enum
  unsigned Access : 2;
  friend class CXXClassMemberWrapper;
  
  // PCHLevel - the "level" of precompiled header/AST file from which this
  // declaration was built.
  unsigned PCHLevel : 2;
  
  /// IdentifierNamespace - This specifies what IDNS_* namespace this lives in.
  unsigned IdentifierNamespace : 16;

private:
#ifndef NDEBUG
  void CheckAccessDeclContext() const;
#else
  void CheckAccessDeclContext() const { }
#endif

protected:

  Decl(Kind DK, DeclContext *DC, SourceLocation L)
    : NextDeclInContext(0), DeclCtx(DC),
      Loc(L), DeclKind(DK), InvalidDecl(0),
      HasAttrs(false), Implicit(false), Used(false),
      Access(AS_none), PCHLevel(0),
      IdentifierNamespace(getIdentifierNamespaceForKind(DK)) {
    if (Decl::CollectingStats()) addDeclKind(DK);
  }

  virtual ~Decl();

public:

  /// \brief Source range that this declaration covers.
  virtual SourceRange getSourceRange() const {
    return SourceRange(getLocation(), getLocation());
  }
  SourceLocation getLocStart() const { return getSourceRange().getBegin(); }
  SourceLocation getLocEnd() const { return getSourceRange().getEnd(); }

  SourceLocation getLocation() const { return Loc; }
  void setLocation(SourceLocation L) { Loc = L; }

  Kind getKind() const { return DeclKind; }
  const char *getDeclKindName() const;

  Decl *getNextDeclInContext() { return NextDeclInContext; }
  const Decl *getNextDeclInContext() const { return NextDeclInContext; }

  DeclContext *getDeclContext() {
    if (isInSemaDC())
      return getSemanticDC();
    return getMultipleDC()->SemanticDC;
  }
  const DeclContext *getDeclContext() const {
    return const_cast<Decl*>(this)->getDeclContext();
  }

  TranslationUnitDecl *getTranslationUnitDecl();
  const TranslationUnitDecl *getTranslationUnitDecl() const {
    return const_cast<Decl*>(this)->getTranslationUnitDecl();
  }

  bool isInAnonymousNamespace() const;

  ASTContext &getASTContext() const;

  void setAccess(AccessSpecifier AS) {
    Access = AS;
    CheckAccessDeclContext();
  }

  AccessSpecifier getAccess() const {
    CheckAccessDeclContext();
    return AccessSpecifier(Access);
  }

  bool hasAttrs() const { return HasAttrs; }
  void addAttr(Attr *attr);
  const Attr *getAttrs() const {
    if (!HasAttrs) return 0;  // common case, no attributes.
    return getAttrsImpl();    // Uncommon case, out of line hash lookup.
  }
  void swapAttrs(Decl *D);
  void invalidateAttrs();

  template<typename T> const T *getAttr() const {
    for (const Attr *attr = getAttrs(); attr; attr = attr->getNext())
      if (const T *V = dyn_cast<T>(attr))
        return V;
    return 0;
  }

  template<typename T> bool hasAttr() const {
    return getAttr<T>() != 0;
  }

  /// setInvalidDecl - Indicates the Decl had a semantic error. This
  /// allows for graceful error recovery.
  void setInvalidDecl(bool Invalid = true) { InvalidDecl = Invalid; }
  bool isInvalidDecl() const { return (bool) InvalidDecl; }

  /// isImplicit - Indicates whether the declaration was implicitly
  /// generated by the implementation. If false, this declaration
  /// was written explicitly in the source code.
  bool isImplicit() const { return Implicit; }
  void setImplicit(bool I = true) { Implicit = I; }

  /// \brief Whether this declaration was used, meaning that a definition
  /// is required.
  bool isUsed() const { return Used; }
  void setUsed(bool U = true) { Used = U; }

  /// \brief Retrieve the level of precompiled header from which this
  /// declaration was generated.
  ///
  /// The PCH level of a declaration describes where the declaration originated
  /// from. A PCH level of 0 indicates that the declaration was not from a 
  /// precompiled header. A PCH level of 1 indicates that the declaration was
  /// from a top-level precompiled header; 2 indicates that the declaration 
  /// comes from a precompiled header on which the top-level precompiled header
  /// depends, and so on. 
  unsigned getPCHLevel() const { return PCHLevel; }

  /// \brief The maximum PCH level that any declaration may have.
  static const unsigned MaxPCHLevel = 3;
  
  /// \brief Set the PCH level of this declaration.
  void setPCHLevel(unsigned Level) { 
    assert(Level < MaxPCHLevel && "PCH level exceeds the maximum");
    PCHLevel = Level;
  }
  
  unsigned getIdentifierNamespace() const {
    return IdentifierNamespace;
  }
  bool isInIdentifierNamespace(unsigned NS) const {
    return getIdentifierNamespace() & NS;
  }
  static unsigned getIdentifierNamespaceForKind(Kind DK);


  /// getLexicalDeclContext - The declaration context where this Decl was
  /// lexically declared (LexicalDC). May be different from
  /// getDeclContext() (SemanticDC).
  /// e.g.:
  ///
  ///   namespace A {
  ///      void f(); // SemanticDC == LexicalDC == 'namespace A'
  ///   }
  ///   void A::f(); // SemanticDC == namespace 'A'
  ///                // LexicalDC == global namespace
  DeclContext *getLexicalDeclContext() {
    if (isInSemaDC())
      return getSemanticDC();
    return getMultipleDC()->LexicalDC;
  }
  const DeclContext *getLexicalDeclContext() const {
    return const_cast<Decl*>(this)->getLexicalDeclContext();
  }

  bool isOutOfLine() const {
    return getLexicalDeclContext() != getDeclContext();
  }

  /// setDeclContext - Set both the semantic and lexical DeclContext
  /// to DC.
  void setDeclContext(DeclContext *DC);

  void setLexicalDeclContext(DeclContext *DC);

  // isDefinedOutsideFunctionOrMethod - This predicate returns true if this
  // scoped decl is defined outside the current function or method.  This is
  // roughly global variables and functions, but also handles enums (which could
  // be defined inside or outside a function etc).
  bool isDefinedOutsideFunctionOrMethod() const;

  /// \brief Retrieves the "canonical" declaration of the given declaration.
  virtual Decl *getCanonicalDecl() { return this; }
  const Decl *getCanonicalDecl() const {
    return const_cast<Decl*>(this)->getCanonicalDecl();
  }

  /// \brief Whether this particular Decl is a canonical one.
  bool isCanonicalDecl() const { return getCanonicalDecl() == this; }

protected:
  /// \brief Returns the next redeclaration or itself if this is the only decl.
  ///
  /// Decl subclasses that can be redeclared should override this method so that
  /// Decl::redecl_iterator can iterate over them.
  virtual Decl *getNextRedeclaration() { return this; }

public:
  /// \brief Iterates through all the redeclarations of the same decl.
  class redecl_iterator {
    /// Current - The current declaration.
    Decl *Current;
    Decl *Starter;

  public:
    typedef Decl*                     value_type;
    typedef Decl*                     reference;
    typedef Decl*                     pointer;
    typedef std::forward_iterator_tag iterator_category;
    typedef std::ptrdiff_t            difference_type;

    redecl_iterator() : Current(0) { }
    explicit redecl_iterator(Decl *C) : Current(C), Starter(C) { }

    reference operator*() const { return Current; }
    pointer operator->() const { return Current; }

    redecl_iterator& operator++() {
      assert(Current && "Advancing while iterator has reached end");
      // Get either previous decl or latest decl.
      Decl *Next = Current->getNextRedeclaration();
      assert(Next && "Should return next redeclaration or itself, never null!");
      Current = (Next != Starter ? Next : 0);
      return *this;
    }

    redecl_iterator operator++(int) {
      redecl_iterator tmp(*this);
      ++(*this);
      return tmp;
    }

    friend bool operator==(redecl_iterator x, redecl_iterator y) {
      return x.Current == y.Current;
    }
    friend bool operator!=(redecl_iterator x, redecl_iterator y) {
      return x.Current != y.Current;
    }
  };

  /// \brief Returns iterator for all the redeclarations of the same decl.
  /// It will iterate at least once (when this decl is the only one).
  redecl_iterator redecls_begin() const {
    return redecl_iterator(const_cast<Decl*>(this));
  }
  redecl_iterator redecls_end() const { return redecl_iterator(); }

  /// getBody - If this Decl represents a declaration for a body of code,
  ///  such as a function or method definition, this method returns the
  ///  top-level Stmt* of that body.  Otherwise this method returns null.
  virtual Stmt* getBody() const { return 0; }

  /// getCompoundBody - Returns getBody(), dyn_casted to a CompoundStmt.
  CompoundStmt* getCompoundBody() const;

  /// getBodyRBrace - Gets the right brace of the body, if a body exists.
  /// This works whether the body is a CompoundStmt or a CXXTryStmt.
  SourceLocation getBodyRBrace() const;

  // global temp stats (until we have a per-module visitor)
  static void addDeclKind(Kind k);
  static bool CollectingStats(bool Enable = false);
  static void PrintStats();

  /// isTemplateParameter - Determines whether this declaration is a
  /// template parameter.
  bool isTemplateParameter() const;

  /// isTemplateParameter - Determines whether this declaration is a
  /// template parameter pack.
  bool isTemplateParameterPack() const;

  /// \brief Whether this declaration is a function or function template.
  bool isFunctionOrFunctionTemplate() const;

  /// \brief Changes the namespace of this declaration to reflect that it's
  /// the object of a friend declaration.
  ///
  /// These declarations appear in the lexical context of the friending
  /// class, but in the semantic context of the actual entity.  This property
  /// applies only to a specific decl object;  other redeclarations of the
  /// same entity may not (and probably don't) share this property.
  void setObjectOfFriendDecl(bool PreviouslyDeclared) {
    unsigned OldNS = IdentifierNamespace;
    assert((OldNS == IDNS_Tag || OldNS == IDNS_Ordinary ||
            OldNS == (IDNS_Tag | IDNS_Ordinary))
           && "unsupported namespace for undeclared friend");
    if (!PreviouslyDeclared) IdentifierNamespace = 0;

    if (OldNS == IDNS_Tag)
      IdentifierNamespace |= IDNS_TagFriend;
    else
      IdentifierNamespace |= IDNS_OrdinaryFriend;
  }

  enum FriendObjectKind {
    FOK_None, // not a friend object
    FOK_Declared, // a friend of a previously-declared entity
    FOK_Undeclared // a friend of a previously-undeclared entity
  };

  /// \brief Determines whether this declaration is the object of a
  /// friend declaration and, if so, what kind.
  ///
  /// There is currently no direct way to find the associated FriendDecl.
  FriendObjectKind getFriendObjectKind() const {
    unsigned mask
      = (IdentifierNamespace & (IDNS_TagFriend | IDNS_OrdinaryFriend));
    if (!mask) return FOK_None;
    return (IdentifierNamespace & (IDNS_Tag | IDNS_Ordinary) ? 
              FOK_Declared : FOK_Undeclared);
  }

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Decl *) { return true; }
  static DeclContext *castToDeclContext(const Decl *);
  static Decl *castFromDeclContext(const DeclContext *);

  /// Destroy - Call destructors and release memory.
  virtual void Destroy(ASTContext& C);

  void print(llvm::raw_ostream &Out, unsigned Indentation = 0) const;
  void print(llvm::raw_ostream &Out, const PrintingPolicy &Policy,
             unsigned Indentation = 0) const;
  static void printGroup(Decl** Begin, unsigned NumDecls,
                         llvm::raw_ostream &Out, const PrintingPolicy &Policy,
                         unsigned Indentation = 0);
  void dump() const;

private:
  const Attr *getAttrsImpl() const;

};

/// PrettyStackTraceDecl - If a crash occurs, indicate that it happened when
/// doing something to a specific decl.
class PrettyStackTraceDecl : public llvm::PrettyStackTraceEntry {
  const Decl *TheDecl;
  SourceLocation Loc;
  SourceManager &SM;
  const char *Message;
public:
  PrettyStackTraceDecl(const Decl *theDecl, SourceLocation L,
                       SourceManager &sm, const char *Msg)
  : TheDecl(theDecl), Loc(L), SM(sm), Message(Msg) {}

  virtual void print(llvm::raw_ostream &OS) const;
};


/// DeclContext - This is used only as base class of specific decl types that
/// can act as declaration contexts. These decls are (only the top classes
/// that directly derive from DeclContext are mentioned, not their subclasses):
///
///   TranslationUnitDecl
///   NamespaceDecl
///   FunctionDecl
///   TagDecl
///   ObjCMethodDecl
///   ObjCContainerDecl
///   LinkageSpecDecl
///   BlockDecl
///
class DeclContext {
  /// DeclKind - This indicates which class this is.
  Decl::Kind DeclKind   :  8;

  /// \brief Whether this declaration context also has some external
  /// storage that contains additional declarations that are lexically
  /// part of this context.
  mutable bool ExternalLexicalStorage : 1;

  /// \brief Whether this declaration context also has some external
  /// storage that contains additional declarations that are visible
  /// in this context.
  mutable bool ExternalVisibleStorage : 1;

  /// \brief Pointer to the data structure used to lookup declarations
  /// within this context, which is a DenseMap<DeclarationName,
  /// StoredDeclsList>.
  mutable void* LookupPtr;

  /// FirstDecl - The first declaration stored within this declaration
  /// context.
  mutable Decl *FirstDecl;

  /// LastDecl - The last declaration stored within this declaration
  /// context. FIXME: We could probably cache this value somewhere
  /// outside of the DeclContext, to reduce the size of DeclContext by
  /// another pointer.
  mutable Decl *LastDecl;

protected:
   DeclContext(Decl::Kind K)
     : DeclKind(K), ExternalLexicalStorage(false),
       ExternalVisibleStorage(false), LookupPtr(0), FirstDecl(0),
       LastDecl(0) { }

  void DestroyDecls(ASTContext &C);

public:
  ~DeclContext();

  Decl::Kind getDeclKind() const {
    return DeclKind;
  }
  const char *getDeclKindName() const;

  /// getParent - Returns the containing DeclContext.
  DeclContext *getParent() {
    return cast<Decl>(this)->getDeclContext();
  }
  const DeclContext *getParent() const {
    return const_cast<DeclContext*>(this)->getParent();
  }

  /// getLexicalParent - Returns the containing lexical DeclContext. May be
  /// different from getParent, e.g.:
  ///
  ///   namespace A {
  ///      struct S;
  ///   }
  ///   struct A::S {}; // getParent() == namespace 'A'
  ///                   // getLexicalParent() == translation unit
  ///
  DeclContext *getLexicalParent() {
    return cast<Decl>(this)->getLexicalDeclContext();
  }
  const DeclContext *getLexicalParent() const {
    return const_cast<DeclContext*>(this)->getLexicalParent();
  }

  DeclContext *getLookupParent();
  
  const DeclContext *getLookupParent() const {
    return const_cast<DeclContext*>(this)->getLookupParent();
  }
  
  ASTContext &getParentASTContext() const {
    return cast<Decl>(this)->getASTContext();
  }

  bool isFunctionOrMethod() const {
    switch (DeclKind) {
    case Decl::Block:
    case Decl::ObjCMethod:
      return true;
    default:
      return DeclKind >= Decl::FunctionFirst && DeclKind <= Decl::FunctionLast;
    }
  }

  bool isFileContext() const {
    return DeclKind == Decl::TranslationUnit || DeclKind == Decl::Namespace;
  }

  bool isTranslationUnit() const {
    return DeclKind == Decl::TranslationUnit;
  }

  bool isRecord() const {
    return DeclKind >= Decl::RecordFirst && DeclKind <= Decl::RecordLast;
  }

  bool isNamespace() const {
    return DeclKind == Decl::Namespace;
  }

  /// \brief Determines whether this context is dependent on a
  /// template parameter.
  bool isDependentContext() const;

  /// isTransparentContext - Determines whether this context is a
  /// "transparent" context, meaning that the members declared in this
  /// context are semantically declared in the nearest enclosing
  /// non-transparent (opaque) context but are lexically declared in
  /// this context. For example, consider the enumerators of an
  /// enumeration type:
  /// @code
  /// enum E {
  ///   Val1
  /// };
  /// @endcode
  /// Here, E is a transparent context, so its enumerator (Val1) will
  /// appear (semantically) that it is in the same context of E.
  /// Examples of transparent contexts include: enumerations (except for
  /// C++0x scoped enums), C++ linkage specifications, and C++0x
  /// inline namespaces.
  bool isTransparentContext() const;

  /// \brief Determine whether this declaration context is equivalent
  /// to the declaration context DC.
  bool Equals(DeclContext *DC) {
    return this->getPrimaryContext() == DC->getPrimaryContext();
  }

  /// \brief Determine whether this declaration context encloses the
  /// declaration context DC.
  bool Encloses(DeclContext *DC);

  /// getPrimaryContext - There may be many different
  /// declarations of the same entity (including forward declarations
  /// of classes, multiple definitions of namespaces, etc.), each with
  /// a different set of declarations. This routine returns the
  /// "primary" DeclContext structure, which will contain the
  /// information needed to perform name lookup into this context.
  DeclContext *getPrimaryContext();

  /// getLookupContext - Retrieve the innermost non-transparent
  /// context of this context, which corresponds to the innermost
  /// location from which name lookup can find the entities in this
  /// context.
  DeclContext *getLookupContext();
  const DeclContext *getLookupContext() const {
    return const_cast<DeclContext *>(this)->getLookupContext();
  }

  /// \brief Retrieve the nearest enclosing namespace context.
  DeclContext *getEnclosingNamespaceContext();
  const DeclContext *getEnclosingNamespaceContext() const {
    return const_cast<DeclContext *>(this)->getEnclosingNamespaceContext();
  }

  /// getNextContext - If this is a DeclContext that may have other
  /// DeclContexts that are semantically connected but syntactically
  /// different, such as C++ namespaces, this routine retrieves the
  /// next DeclContext in the link. Iteration through the chain of
  /// DeclContexts should begin at the primary DeclContext and
  /// continue until this function returns NULL. For example, given:
  /// @code
  /// namespace N {
  ///   int x;
  /// }
  /// namespace N {
  ///   int y;
  /// }
  /// @endcode
  /// The first occurrence of namespace N will be the primary
  /// DeclContext. Its getNextContext will return the second
  /// occurrence of namespace N.
  DeclContext *getNextContext();

  /// decl_iterator - Iterates through the declarations stored
  /// within this context.
  class decl_iterator {
    /// Current - The current declaration.
    Decl *Current;

  public:
    typedef Decl*                     value_type;
    typedef Decl*                     reference;
    typedef Decl*                     pointer;
    typedef std::forward_iterator_tag iterator_category;
    typedef std::ptrdiff_t            difference_type;

    decl_iterator() : Current(0) { }
    explicit decl_iterator(Decl *C) : Current(C) { }

    reference operator*() const { return Current; }
    pointer operator->() const { return Current; }

    decl_iterator& operator++() {
      Current = Current->getNextDeclInContext();
      return *this;
    }

    decl_iterator operator++(int) {
      decl_iterator tmp(*this);
      ++(*this);
      return tmp;
    }

    friend bool operator==(decl_iterator x, decl_iterator y) {
      return x.Current == y.Current;
    }
    friend bool operator!=(decl_iterator x, decl_iterator y) {
      return x.Current != y.Current;
    }
  };

  /// decls_begin/decls_end - Iterate over the declarations stored in
  /// this context.
  decl_iterator decls_begin() const;
  decl_iterator decls_end() const;
  bool decls_empty() const;

  /// specific_decl_iterator - Iterates over a subrange of
  /// declarations stored in a DeclContext, providing only those that
  /// are of type SpecificDecl (or a class derived from it). This
  /// iterator is used, for example, to provide iteration over just
  /// the fields within a RecordDecl (with SpecificDecl = FieldDecl).
  template<typename SpecificDecl>
  class specific_decl_iterator {
    /// Current - The current, underlying declaration iterator, which
    /// will either be NULL or will point to a declaration of
    /// type SpecificDecl.
    DeclContext::decl_iterator Current;

    /// SkipToNextDecl - Advances the current position up to the next
    /// declaration of type SpecificDecl that also meets the criteria
    /// required by Acceptable.
    void SkipToNextDecl() {
      while (*Current && !isa<SpecificDecl>(*Current))
        ++Current;
    }

  public:
    typedef SpecificDecl* value_type;
    typedef SpecificDecl* reference;
    typedef SpecificDecl* pointer;
    typedef std::iterator_traits<DeclContext::decl_iterator>::difference_type
      difference_type;
    typedef std::forward_iterator_tag iterator_category;

    specific_decl_iterator() : Current() { }

    /// specific_decl_iterator - Construct a new iterator over a
    /// subset of the declarations the range [C,
    /// end-of-declarations). If A is non-NULL, it is a pointer to a
    /// member function of SpecificDecl that should return true for
    /// all of the SpecificDecl instances that will be in the subset
    /// of iterators. For example, if you want Objective-C instance
    /// methods, SpecificDecl will be ObjCMethodDecl and A will be
    /// &ObjCMethodDecl::isInstanceMethod.
    explicit specific_decl_iterator(DeclContext::decl_iterator C) : Current(C) {
      SkipToNextDecl();
    }

    reference operator*() const { return cast<SpecificDecl>(*Current); }
    pointer operator->() const { return cast<SpecificDecl>(*Current); }

    specific_decl_iterator& operator++() {
      ++Current;
      SkipToNextDecl();
      return *this;
    }

    specific_decl_iterator operator++(int) {
      specific_decl_iterator tmp(*this);
      ++(*this);
      return tmp;
    }

    friend bool
    operator==(const specific_decl_iterator& x, const specific_decl_iterator& y) {
      return x.Current == y.Current;
    }

    friend bool
    operator!=(const specific_decl_iterator& x, const specific_decl_iterator& y) {
      return x.Current != y.Current;
    }
  };

  /// \brief Iterates over a filtered subrange of declarations stored
  /// in a DeclContext.
  ///
  /// This iterator visits only those declarations that are of type
  /// SpecificDecl (or a class derived from it) and that meet some
  /// additional run-time criteria. This iterator is used, for
  /// example, to provide access to the instance methods within an
  /// Objective-C interface (with SpecificDecl = ObjCMethodDecl and
  /// Acceptable = ObjCMethodDecl::isInstanceMethod).
  template<typename SpecificDecl, bool (SpecificDecl::*Acceptable)() const>
  class filtered_decl_iterator {
    /// Current - The current, underlying declaration iterator, which
    /// will either be NULL or will point to a declaration of
    /// type SpecificDecl.
    DeclContext::decl_iterator Current;

    /// SkipToNextDecl - Advances the current position up to the next
    /// declaration of type SpecificDecl that also meets the criteria
    /// required by Acceptable.
    void SkipToNextDecl() {
      while (*Current &&
             (!isa<SpecificDecl>(*Current) ||
              (Acceptable && !(cast<SpecificDecl>(*Current)->*Acceptable)())))
        ++Current;
    }

  public:
    typedef SpecificDecl* value_type;
    typedef SpecificDecl* reference;
    typedef SpecificDecl* pointer;
    typedef std::iterator_traits<DeclContext::decl_iterator>::difference_type
      difference_type;
    typedef std::forward_iterator_tag iterator_category;

    filtered_decl_iterator() : Current() { }

    /// specific_decl_iterator - Construct a new iterator over a
    /// subset of the declarations the range [C,
    /// end-of-declarations). If A is non-NULL, it is a pointer to a
    /// member function of SpecificDecl that should return true for
    /// all of the SpecificDecl instances that will be in the subset
    /// of iterators. For example, if you want Objective-C instance
    /// methods, SpecificDecl will be ObjCMethodDecl and A will be
    /// &ObjCMethodDecl::isInstanceMethod.
    explicit filtered_decl_iterator(DeclContext::decl_iterator C) : Current(C) {
      SkipToNextDecl();
    }

    reference operator*() const { return cast<SpecificDecl>(*Current); }
    pointer operator->() const { return cast<SpecificDecl>(*Current); }

    filtered_decl_iterator& operator++() {
      ++Current;
      SkipToNextDecl();
      return *this;
    }

    filtered_decl_iterator operator++(int) {
      filtered_decl_iterator tmp(*this);
      ++(*this);
      return tmp;
    }

    friend bool
    operator==(const filtered_decl_iterator& x, const filtered_decl_iterator& y) {
      return x.Current == y.Current;
    }

    friend bool
    operator!=(const filtered_decl_iterator& x, const filtered_decl_iterator& y) {
      return x.Current != y.Current;
    }
  };

  /// @brief Add the declaration D into this context.
  ///
  /// This routine should be invoked when the declaration D has first
  /// been declared, to place D into the context where it was
  /// (lexically) defined. Every declaration must be added to one
  /// (and only one!) context, where it can be visited via
  /// [decls_begin(), decls_end()). Once a declaration has been added
  /// to its lexical context, the corresponding DeclContext owns the
  /// declaration.
  ///
  /// If D is also a NamedDecl, it will be made visible within its
  /// semantic context via makeDeclVisibleInContext.
  void addDecl(Decl *D);

  /// @brief Add the declaration D to this context without modifying
  /// any lookup tables.
  ///
  /// This is useful for some operations in dependent contexts where
  /// the semantic context might not be dependent;  this basically
  /// only happens with friends.
  void addHiddenDecl(Decl *D);

  /// @brief Removes a declaration from this context.
  void removeDecl(Decl *D);

  /// lookup_iterator - An iterator that provides access to the results
  /// of looking up a name within this context.
  typedef NamedDecl **lookup_iterator;

  /// lookup_const_iterator - An iterator that provides non-mutable
  /// access to the results of lookup up a name within this context.
  typedef NamedDecl * const * lookup_const_iterator;

  typedef std::pair<lookup_iterator, lookup_iterator> lookup_result;
  typedef std::pair<lookup_const_iterator, lookup_const_iterator>
    lookup_const_result;

  /// lookup - Find the declarations (if any) with the given Name in
  /// this context. Returns a range of iterators that contains all of
  /// the declarations with this name, with object, function, member,
  /// and enumerator names preceding any tag name. Note that this
  /// routine will not look into parent contexts.
  lookup_result lookup(DeclarationName Name);
  lookup_const_result lookup(DeclarationName Name) const;

  /// @brief Makes a declaration visible within this context.
  ///
  /// This routine makes the declaration D visible to name lookup
  /// within this context and, if this is a transparent context,
  /// within its parent contexts up to the first enclosing
  /// non-transparent context. Making a declaration visible within a
  /// context does not transfer ownership of a declaration, and a
  /// declaration can be visible in many contexts that aren't its
  /// lexical context.
  ///
  /// If D is a redeclaration of an existing declaration that is
  /// visible from this context, as determined by
  /// NamedDecl::declarationReplaces, the previous declaration will be
  /// replaced with D.
  ///
  /// @param Recoverable true if it's okay to not add this decl to
  /// the lookup tables because it can be easily recovered by walking
  /// the declaration chains.
  void makeDeclVisibleInContext(NamedDecl *D, bool Recoverable = true);

  /// udir_iterator - Iterates through the using-directives stored
  /// within this context.
  typedef UsingDirectiveDecl * const * udir_iterator;

  typedef std::pair<udir_iterator, udir_iterator> udir_iterator_range;

  udir_iterator_range getUsingDirectives() const;

  udir_iterator using_directives_begin() const {
    return getUsingDirectives().first;
  }

  udir_iterator using_directives_end() const {
    return getUsingDirectives().second;
  }

  // Low-level accessors

  /// \brief Retrieve the internal representation of the lookup structure.
  void* getLookupPtr() const { return LookupPtr; }

  /// \brief Whether this DeclContext has external storage containing
  /// additional declarations that are lexically in this context.
  bool hasExternalLexicalStorage() const { return ExternalLexicalStorage; }

  /// \brief State whether this DeclContext has external storage for
  /// declarations lexically in this context.
  void setHasExternalLexicalStorage(bool ES = true) {
    ExternalLexicalStorage = ES;
  }

  /// \brief Whether this DeclContext has external storage containing
  /// additional declarations that are visible in this context.
  bool hasExternalVisibleStorage() const { return ExternalVisibleStorage; }

  /// \brief State whether this DeclContext has external storage for
  /// declarations visible in this context.
  void setHasExternalVisibleStorage(bool ES = true) {
    ExternalVisibleStorage = ES;
  }

  static bool classof(const Decl *D);
  static bool classof(const DeclContext *D) { return true; }
#define DECL_CONTEXT(Name) \
  static bool classof(const Name##Decl *D) { return true; }
#include "clang/AST/DeclNodes.def"

  void dumpDeclContext() const;

private:
  void LoadLexicalDeclsFromExternalStorage() const;
  void LoadVisibleDeclsFromExternalStorage() const;

  void buildLookup(DeclContext *DCtx);
  void makeDeclVisibleInContextImpl(NamedDecl *D);
};

inline bool Decl::isTemplateParameter() const {
  return getKind() == TemplateTypeParm || getKind() == NonTypeTemplateParm;
}

inline bool Decl::isDefinedOutsideFunctionOrMethod() const {
  if (getDeclContext())
    return !getDeclContext()->getLookupContext()->isFunctionOrMethod();
  return true;
}

} // end clang.

namespace llvm {

/// Implement a isa_impl_wrap specialization to check whether a DeclContext is
/// a specific Decl.
template<class ToTy>
struct isa_impl_wrap<ToTy,
                     const ::clang::DeclContext,const ::clang::DeclContext> {
  static bool doit(const ::clang::DeclContext &Val) {
    return ToTy::classof(::clang::Decl::castFromDeclContext(&Val));
  }
};
template<class ToTy>
struct isa_impl_wrap<ToTy, ::clang::DeclContext, ::clang::DeclContext>
  : public isa_impl_wrap<ToTy,
                      const ::clang::DeclContext,const ::clang::DeclContext> {};

/// Implement cast_convert_val for Decl -> DeclContext conversions.
template<class FromTy>
struct cast_convert_val< ::clang::DeclContext, FromTy, FromTy> {
  static ::clang::DeclContext &doit(const FromTy &Val) {
    return *FromTy::castToDeclContext(&Val);
  }
};

template<class FromTy>
struct cast_convert_val< ::clang::DeclContext, FromTy*, FromTy*> {
  static ::clang::DeclContext *doit(const FromTy *Val) {
    return FromTy::castToDeclContext(Val);
  }
};

template<class FromTy>
struct cast_convert_val< const ::clang::DeclContext, FromTy, FromTy> {
  static const ::clang::DeclContext &doit(const FromTy &Val) {
    return *FromTy::castToDeclContext(&Val);
  }
};

template<class FromTy>
struct cast_convert_val< const ::clang::DeclContext, FromTy*, FromTy*> {
  static const ::clang::DeclContext *doit(const FromTy *Val) {
    return FromTy::castToDeclContext(Val);
  }
};

/// Implement cast_convert_val for DeclContext -> Decl conversions.
template<class ToTy>
struct cast_convert_val<ToTy,
                        const ::clang::DeclContext,const ::clang::DeclContext> {
  static ToTy &doit(const ::clang::DeclContext &Val) {
    return *reinterpret_cast<ToTy*>(ToTy::castFromDeclContext(&Val));
  }
};
template<class ToTy>
struct cast_convert_val<ToTy, ::clang::DeclContext, ::clang::DeclContext>
  : public cast_convert_val<ToTy,
                      const ::clang::DeclContext,const ::clang::DeclContext> {};

template<class ToTy>
struct cast_convert_val<ToTy,
                     const ::clang::DeclContext*, const ::clang::DeclContext*> {
  static ToTy *doit(const ::clang::DeclContext *Val) {
    return reinterpret_cast<ToTy*>(ToTy::castFromDeclContext(Val));
  }
};
template<class ToTy>
struct cast_convert_val<ToTy, ::clang::DeclContext*, ::clang::DeclContext*>
  : public cast_convert_val<ToTy,
                    const ::clang::DeclContext*,const ::clang::DeclContext*> {};

} // end namespace llvm

#endif