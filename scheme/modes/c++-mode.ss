(define *c++-keywords*
  (string-append
   "alignas|alignof|and|and_eq|asm|bitand|bitor|catch|class|compl|const|constexpr|const_cast|"
   "decltype|delete|dynamic_cast|explicit|export|false|friend|inline|mutable|namespace|new|noexcept|"
   "not|not_eq|nullptr|operator|or|or_eq|private|public|protected|register|reinterpret_cast|"
   "sizeof|static_assert|static_cast|template|this|thread_local|throw|try|true|typeid|typename|using|"
   "virtual|xor|xor_eq"))

(define-derived-mode c++-mode
  c-mode
  "Mode for editing C++ language."
  (syn 'regex (string-append "\\<(" *c++-keywords* ")\\>") 'keyword-face)
  (syn 'regex "\\s+([A-Za-z_]+)\\s*::" 'type-face))
