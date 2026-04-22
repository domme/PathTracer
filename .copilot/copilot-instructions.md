# PathTracer — Code Style Guide

## General

- C++17
- Headers use `#pragma once`
- Section separators: `//---------------------------------------------------------------------------//`
- Prefer flat includes over deep namespace qualification — bring types into scope with `using namespace Fancy;` (or equivalent) at the top of each file rather than sprinkling `Fancy::` qualifiers throughout

---

## Naming

| Entity | Convention | Example |
|---|---|---|
| Classes / Structs | PascalCase | `GpuBuffer`, `RtAccelerationStructure` |
| Member variables | `my` prefix + PascalCase | `myProperties`, `myNumElements` |
| Function parameters (singular) | `a`/`an` prefix + PascalCase | `aType`, `anOffset`, `aName` |
| Function parameters (plural) | `some` prefix + PascalCase | `someArguments`, `someBuffers` |
| Methods | PascalCase | `GetProperties()`, `Create()` |
| Constants / `enum` fields | ALL_CAPS_WITH_UNDERSCORES | `kNumCachedBarriers`, `PLANE_LEFT` |
| Local variables | camelCase | `startAddress`, `importSuccess` |
| Static / global variables | camelCase | `sceneLoadInfos` |

---

## Types

Use the project-defined type aliases instead of standard-library integers:

```cpp
uint, uint8, uint16, uint64
int16, int64
float64   // double
```

Use `glm` types for math (`glm::float3`, `glm::mat4`, etc.).  
Use EASTL containers (`eastl::vector`, `eastl::fixed_vector`, `eastl::fixed_string`, …).

---

## Spacing — Brackets

Insert a space after every opening bracket and before every closing bracket. This applies to **all bracket types**: `( )`, `[ ]`, and `< >` (template angle brackets).  
Empty bracket pairs are left as-is: `()`, `[]`.

```cpp
// ✅ correct — function calls
alloca( sizeof( D3D12_PLACED_SUBRESOURCE_FOOTPRINT ) * aNumSubresources )
GetDevice()->CreateTexture( desc, someData )

// ✅ correct — C-style casts (space after closing bracket too)
( uint ) someValue
( D3D12_RESOURCE_STATES ) 0u
static_cast< D3D12_PLACED_SUBRESOURCE_FOOTPRINT * >( alloca( ... ) )

// ✅ correct — array subscripts
myBuffers[ i ]
someArray[ index + 1 ]

// ✅ correct — template angle brackets
SharedPtr< GpuBuffer >
eastl::vector< GpuRingBuffer * >
eastl::fixed_vector< glm::float3, 4 >
static_cast< D3D12_PLACED_SUBRESOURCE_FOOTPRINT * >( alloca( ... ) )

// ❌ wrong
alloca(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) * aNumSubresources)
( uint )someValue
myBuffers[i]
SharedPtr<GpuBuffer>
static_cast<D3D12_PLACED_SUBRESOURCE_FOOTPRINT*>(alloca(...))
```

Apply the same rule to function-template call sites and template parameter lists in declarations.

---

## Spacing — Pointers and References

Put the `*` / `&` symbol **next to the type**, with a space separating the symbol from the variable name:

```cpp
// ✅ correct — reference parameter
void Foo(const GpuBufferProperties & someProperties);

// ✅ correct — pointer parameter
void Bar(const GpuBuffer * aBuffer);

// ✅ correct — pointer member / return type
GpuBuffer * GetBuffer() const;
const GpuBuffer * myPtr;

// ❌ wrong — symbol touching the name
void Foo(const GpuBufferProperties &someProperties);
void Bar(const GpuBuffer *aBuffer);
```

**Dereference and address-of operators** in expressions are written with the symbol touching the operand (no leading space, no trailing space before the name):

```cpp
// ✅ correct — dereference / address-of in expressions
*someBuffer
&myResource
**ppData
```

---

## Namespaces

Prefer **not** qualifying types with a namespace prefix in implementation files and in headers that already pull the namespace into scope.  
Bring the namespace into scope once at the top instead:

```cpp
// ✅ preferred in .cpp files
using namespace Fancy;

void Foo(CommandList * aCommandList) { … }

// ❌ avoid unless disambiguation is required
void Foo(Fancy::CommandList * aCommandList) { … }
```

In headers where a `using namespace` would pollute other translation units, use a minimal forward-declaration block or qualify only where necessary.

---

## Classes

- `public` members before `protected` before `private`
- Deleted/defaulted special members are declared explicitly when the intent is non-obvious
- Virtual destructors are marked `= default` where there is no body needed

---

## Comments

Comment only code that genuinely needs clarification. Do not add explanatory prose for self-evident logic.  
Use `// TODO:` for outstanding work items.  
Use the section separator line before and after major declarations inside a namespace:

```cpp
namespace Fancy {
//---------------------------------------------------------------------------//
  class GpuBuffer : public GpuResource
  {
    …
  };
//---------------------------------------------------------------------------//
}
```

---

## Macros and Assertions

Use `ASSERT(condition)` for runtime assertions (not `assert()`).  
Existing helper macros (`SAFE_DELETE`, `ARRAY_LENGTH`, `VECTOR_BYTESIZE`, etc.) are preferred over reimplementing the same logic.

---

## Indentation

- 2 spaces per level
- Contents of a `namespace` block are indented by 2 spaces
- Opening brace `{` goes on the **same line** as the declaration or control statement:

```cpp
// ✅ correct
void SomeFunction() {
}

class Foo {
};

if (condition) {
}

// ❌ wrong
void SomeFunction()
{
}
```
