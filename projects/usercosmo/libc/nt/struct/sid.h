#ifndef COSMOPOLITAN_LIBC_NT_STRUCT_SID_H_
#define COSMOPOLITAN_LIBC_NT_STRUCT_SID_H_

struct NtSidIdentifierAuthority {
  uint8_t Value[6];
};

struct NtSid {
  uint8_t Revision;
  uint8_t SubAuthorityCount;
  struct NtSidIdentifierAuthority IdentifierAuthority;
  uint32_t SubAuthority[1];
};

/* Fil-C port: cosmo repeats the identical struct definition here (GCC 14 in
   C23 mode allows redefining a struct with the same content; GCC 13 and
   clang don't), so under Fil-C we skip the redundant redefinition.  The
   kNtSecurity*SidAuthority macros it (pointlessly) declared are hoisted out
   so they stay unconditional. */
#define kNtSecurityNullSidAuthority         {0, 0, 0, 0, 0, 0}
#define kNtSecurityWorldSidAuthority        {0, 0, 0, 0, 0, 1}
#define kNtSecurityLocalSidAuthority        {0, 0, 0, 0, 0, 2}
#define kNtSecurityCreatorSidAuthority      {0, 0, 0, 0, 0, 3}
#define kNtSecurityNonUniqueAuthority       {0, 0, 0, 0, 0, 4}
#define kNtSecurityResourceManagerAuthority {0, 0, 0, 0, 0, 9}
#ifndef __FILC__
struct NtSidIdentifierAuthority {
  uint8_t Value[6];
};
#endif /* __FILC__ */

#define kNtSecurityNullRid               0x00000000u
#define kNtSecurityWorldRid              0x00000000u
#define kNtSecurityLocalRid              0x00000000u
#define kNtSecurityLocalLogonRid         0x00000001u
#define kNtSecurityCreatorOwnerRid       0x00000000u
#define kNtSecurityCreatorGroupRid       0x00000001u
#define kNtSecurityCreatorOwnerServerRid 0x00000002u
#define kNtSecurityCreatorGroupServerRid 0x00000003u
#define kNtSecurityCreatorOwnerRightsRid 0x00000004u

#endif /* COSMOPOLITAN_LIBC_NT_STRUCT_SID_H_ */
