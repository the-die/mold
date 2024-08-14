#pragma once

#include "common.h"
#include "elf.h"

namespace mold {

enum class FileType {
  UNKNOWN,
  EMPTY,
  ELF_OBJ,
  ELF_DSO,
  AR,
  THIN_AR,
  TEXT,
  GCC_LTO_OBJ,
  LLVM_BITCODE,
};

template <typename MappedFile>
bool is_text_file(MappedFile *mf) {
  auto istext = [](char c) {
    return isprint(c) || c == '\n' || c == '\t';
  };

  u8 *data = mf->data;
  return mf->size >= 4 && istext(data[0]) && istext(data[1]) &&
         istext(data[2]) && istext(data[3]);
}

// https://gcc.gnu.org/onlinedocs/gccint/LTO.html
template <typename E, typename Context, typename MappedFile>
inline bool is_gcc_lto_obj(Context &ctx, MappedFile *mf) {
  const char *data = mf->get_contents().data();
  ElfEhdr<E> &ehdr = *(ElfEhdr<E> *)data;
  ElfShdr<E> *sh_begin = (ElfShdr<E> *)(data + ehdr.e_shoff);
  std::span<ElfShdr<E>> shdrs{(ElfShdr<E> *)(data + ehdr.e_shoff), ehdr.e_shnum};

  // If the index of section name string table section is
  // larger than or equal to SHN_LORESERVE (0xff00), this
  // member holds SHN_XINDEX (0xffff) and the real index of the
  // section name string table section is held in the sh_link
  // member of the initial entry in section header table.
  // Otherwise, the sh_link member of the initial entry in
  // section header table contains the value zero.
  //
  // e_shstrndx is a 16-bit field. If .shstrtab's section index is
  // too large, the actual number is stored to sh_link field.
  i64 shstrtab_idx = (ehdr.e_shstrndx == SHN_XINDEX)
    ? sh_begin->sh_link : ehdr.e_shstrndx;

  for (ElfShdr<E> &sec : shdrs) {
    // -plugin name
    // Involve a plugin in the linking process. The name parameter is the absolute filename of the
    // plugin. Usually this parameter is automatically added by the complier, when using link time
    // optimization, but users can also add their own plugins if they so wish.
    //
    // Note that the location of the compiler originated plugins is different from the place where
    // the ar, nm and ranlib programs search for their plugins. In order for those commands to make
    // use of a compiler based plugin it must first be copied into the ${libdir}/bfd-plugins
    // directory. All gcc based linker plugins are backward compatible, so it is sufficient to just
    // copy in the newest one.
    //
    // GCC FAT LTO objects contain both regular ELF sections and GCC-
    // specific LTO sections, so that they can be linked as LTO objects if
    // the LTO linker plugin is available and falls back as regular
    // objects otherwise. GCC FAT LTO object can be identified by the
    // presence of `.gcc.lto_.symtab` section.
    if (!ctx.arg.plugin.empty()) {
      std::string_view name = data + shdrs[shstrtab_idx].sh_offset + sec.sh_name;
      if (name.starts_with(".gnu.lto_.symtab."))
        return true;
    }

    if (sec.sh_type != SHT_SYMTAB)
      continue;

    // GCC non-FAT LTO object contains only sections symbols followed by
    // a common symbol whose name is `__gnu_lto_slim` (or `__gnu_lto_v1`
    // for older GCC releases).
    std::span<ElfSym<E>> elf_syms{(ElfSym<E> *)(data + sec.sh_offset),
                                  (size_t)sec.sh_size / sizeof(ElfSym<E>)};

    auto skip = [](u8 type) {
      return type == STT_NOTYPE || type == STT_FILE || type == STT_SECTION;
    };

    i64 i = 1;
    while (i < elf_syms.size() && skip(elf_syms[i].st_type))
      i++;

    if (i < elf_syms.size() && elf_syms[i].st_shndx == SHN_COMMON) {
      std::string_view name =
        data + shdrs[sec.sh_link].sh_offset + elf_syms[i].st_name;
      if (name.starts_with("__gnu_lto_"))
        return true;
    }
    break;
  }

  return false;
}

// Infer file type from file content.
template <typename Context, typename MappedFile>
FileType get_file_type(Context &ctx, MappedFile *mf) {
  std::string_view data = mf->get_contents();

  if (data.empty())
    return FileType::EMPTY;

  // https://man7.org/linux/man-pages/man5/elf.5.html
  if (data.starts_with("\177ELF")) {
    u8 byte_order = ((ElfEhdr<I386> *)data.data())->e_ident[EI_DATA];

    if (byte_order == ELFDATA2LSB) {
      auto &ehdr = *(ElfEhdr<I386> *)data.data();

      if (ehdr.e_type == ET_REL) {
        if (ehdr.e_ident[EI_CLASS] == ELFCLASS32) {
          if (is_gcc_lto_obj<I386>(ctx, mf))
            return FileType::GCC_LTO_OBJ;
        } else {
          if (is_gcc_lto_obj<X86_64>(ctx, mf))
            return FileType::GCC_LTO_OBJ;
        }
        return FileType::ELF_OBJ;
      }

      if (ehdr.e_type == ET_DYN)
        return FileType::ELF_DSO;
    } else {
      auto &ehdr = *(ElfEhdr<M68K> *)data.data();

      if (ehdr.e_type == ET_REL) {
        if (ehdr.e_ident[EI_CLASS] == ELFCLASS32) {
          if (is_gcc_lto_obj<M68K>(ctx, mf))
            return FileType::GCC_LTO_OBJ;
        } else {
          if (is_gcc_lto_obj<SPARC64>(ctx, mf))
            return FileType::GCC_LTO_OBJ;
        }
        return FileType::ELF_OBJ;
      }

      if (ehdr.e_type == ET_DYN)
        return FileType::ELF_DSO;
    }
    return FileType::UNKNOWN;
  }

  // https://sourceware.org/git/?p=binutils-gdb.git;a=blob;f=include/aout/ar.h
  // https://sourceware.org/git/?p=binutils-gdb.git;a=blob;f=bfd/archive.c
  // https://sourceware.org/git/?p=binutils-gdb.git;a=blob;f=bfd/archive64.c
  if (data.starts_with("!<arch>\n"))
    return FileType::AR;
  if (data.starts_with("!<thin>\n"))
    return FileType::THIN_AR;
  if (is_text_file(mf))
    return FileType::TEXT;
  if (data.starts_with("\xde\xc0\x17\x0b"))
    return FileType::LLVM_BITCODE;
  if (data.starts_with("BC\xc0\xde"))
    return FileType::LLVM_BITCODE;
  return FileType::UNKNOWN;
}

inline std::string filetype_to_string(FileType type) {
  switch (type) {
  case FileType::UNKNOWN: return "UNKNOWN";
  case FileType::EMPTY: return "EMPTY";
  case FileType::ELF_OBJ: return "ELF_OBJ";
  case FileType::ELF_DSO: return "ELF_DSO";
  case FileType::AR: return "AR";
  case FileType::THIN_AR: return "THIN_AR";
  case FileType::TEXT: return "TEXT";
  case FileType::GCC_LTO_OBJ: return "GCC_LTO_OBJ";
  case FileType::LLVM_BITCODE: return "LLVM_BITCODE";
  }
  return "UNKNOWN";
}

inline std::ostream &operator<<(std::ostream &out, FileType type) {
  out << filetype_to_string(type);
  return out;
}

} // namespace mold
