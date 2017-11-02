#include "cte_typebanks.h"
#include "cte_functypes.h"
#include <clb_verbose.h>

#define REALLOC_STEP       16

#define GetNameId(node)       ((node)->val1.i_val)
#define GetArity(node)        ((node)->val2.i_val)

#define type_a0hash(t) ((t)->f_code)
#define type_a1hash(t) (type_a0hash(t)^(((intptr_t)(t)->args[0])>>3))
#define type_aritynhash(t) (type_a1hash(t)^(((intptr_t)(t)->args[1])>>4))

typedef struct back_idx_info
{
   const char* name;
   int         arity;
} back_idx_info;

Type_p parse_single_type(Scanner_p in, TypeBank_p bank);
FuncSymbType TermParseOperator(Scanner_p in, DStr_p id);

static __inline__ back_idx_info* bii_alloc(const char* name, int arity)
{
   back_idx_info* ptr = SizeMalloc(sizeof(back_idx_info));
   ptr->name  = SecureStrdup(name);
   ptr->arity = arity;
   return ptr;
}

#define hash_type(type)\
((((type)->arity == 0)?\
  type_a0hash(type):\
  (((type)->arity == 1)?type_a1hash(type):type_aritynhash(type)))\
   &(TYPEBANK_HASH_MASK))


int cmp_types(const void* el1, const void* el2)
{
   assert(el1 && el2);
   Type_p t1 = (Type_p)el1;
   Type_p t2 = (Type_p)el2;

   int res =  TypesCmp(t1 , t2);

   return res;
}


static __inline__ void type_arg_realloc(Type_p** args, int current, int new)
{
  Type_p* new_arr = SizeMalloc(new * sizeof(Type_p));

  int min_size = MIN(current, new);
  for(int i=0; i<min_size; i++)
  {
    new_arr[i] = (*args)[i];
  }

  SizeFree(*args, current * sizeof(Type_p));
  *args = new_arr;
}



TypeBank_p TypeBankAlloc()
{
   TypeBank_p handle = TypeBankCellAlloc();

   handle->back_idx = PStackAlloc();
   handle->name_idx = NULL;

   handle->names_count = 0;
   handle->types_count = 0;

   for(int i = 0; i < TYPEBANK_SIZE; i++)
   {
      handle->hash_table[i] = NULL;
   }

   TypeConsCode res = TypeBankDefineSimpleSort(handle, "$>_type");
   UNUSED(res); assert(res == ArrowTypeCons);
   res = TypeBankDefineSimpleSort(handle, "$o");
   UNUSED(res); assert(res == STBool);
   res = TypeBankDefineSimpleSort(handle, "$i");
   UNUSED(res); assert(res == STIndividuals);
   res = TypeBankDefineSimpleSort(handle, "$tType");
   UNUSED(res); assert(res == STKind);
   res = TypeBankDefineSimpleSort(handle, "$int");
   UNUSED(res); assert(res == STInteger);
   res = TypeBankDefineSimpleSort(handle, "$rat");
   UNUSED(res); assert(res == STRational);
   res = TypeBankDefineSimpleSort(handle, "$real");
   UNUSED(res); assert(res == STReal);

   handle->bool_type       = TypeBankInsertTypeShared(handle, AllocSimpleSort(STBool));
   handle->i_type          = TypeBankInsertTypeShared(handle, AllocSimpleSort(STIndividuals));
   handle->kind_type       = TypeBankInsertTypeShared(handle, AllocSimpleSort(STKind));
   handle->integer_type    = TypeBankInsertTypeShared(handle, AllocSimpleSort(STInteger));
   handle->rational_type   = TypeBankInsertTypeShared(handle, AllocSimpleSort(STRational));
   handle->real_type       = TypeBankInsertTypeShared(handle, AllocSimpleSort(STReal));

   handle->default_type = handle->i_type;

   return handle;
}

void __inline__ handle_args(TypeBank_p bank, Type_p t)
{
   assert(bank);
   assert(t);

   for(int i=0; i<t->arity; i++)
   {
      assert(t->args[i]);
      t->args[i] = t->args[i]->type_uid == INVALID_TYPE_UID ?
                   TypeBankInsertTypeShared(bank, t->args[i]) : t->args[i];
   }
}

Type_p TypeBankInsertTypeShared(TypeBank_p bank, Type_p t)
{
   assert(bank);
   assert(t);
   Type_p res;

   handle_args(bank, t);
   if (t->type_uid == INVALID_TYPE_UID)
   {
      res = (Type_p) PTreeObjStore(&(bank->hash_table[hash_type(t)]), t, cmp_types);

      if (res == NULL)
      {
         res = t;
         res->type_uid = ++bank->types_count;
         if (Verbose > 1)
         {
            fprintf(stderr, "# Had to insert type ");
            TypePrintTSTP(stderr, bank, t);
            fprintf(stderr, ".\n");
         }
      }
      else
      {         
         TypeFree(t);
      }
   }
   else
   {
      assert(PTreeObjFind(bank->hash_table + hash_type(t), t, cmp_types));
      res = t;
   }

   return res;
}


TypeConsCode TypeBankDefineTypeConstructor(TypeBank_p bank, const char* name, int arity)
{
   assert(bank);
   StrTree_p node = StrTreeFind(&bank->name_idx, name);
   if (node)
   {
      if (GetArity(node) == arity)
      {
         return GetNameId(node);
      }
      else
      {
         DStr_p err_msg = DStrAlloc();
         DStrAppendStr(err_msg, (char*)"Redefinition of type constructor ");
         DStrAppendStr(err_msg, (char*)name);

         if (Verbose > 1)
         {
            fprintf(stderr, "# previous arity was %ld, now it is %d.\n", GetArity(node), arity);
         }

         Error(DStrView(err_msg), SYNTAX_ERROR);
         return -1; // stiff warning
      }
   }
   else 
   {
      IntOrP id    = {.i_val = bank->names_count++};
      IntOrP arity_iop = {.i_val = (long)arity};
      StrTreeStore(&bank->name_idx, (char*)name, id, arity_iop);
      PStackPushP(bank->back_idx, bii_alloc(name, arity));
      assert(PStackGetSP(bank->back_idx) == bank->names_count);

      return id.i_val;
   }
}

TypeConsCode TypeBankDefineSimpleSort(TypeBank_p bank, const char* name)
{
   assert(bank);
   return TypeBankDefineTypeConstructor(bank, name, 0);
}

TypeConsCode TypeBankFindTCCode(TypeBank_p bank, const char* name)
{
   assert(bank);
   StrTree_p node = StrTreeFind(&bank->name_idx, name);
   
   return node ? GetNameId(node) : NAME_NOT_FOUND;
}

int TypeBankFindTCArity(TypeBank_p bank, TypeConsCode tc_code)
{
   assert(tc_code != INVALID_TYPE_UID && tc_code < PStackGetSP(bank->back_idx));
   assert(PStackGetSP(bank->back_idx) == bank->names_count);

   return ((back_idx_info*)PStackElementP(bank->back_idx, tc_code))->arity;
}

const char* TypeBankFindTCName(TypeBank_p bank, TypeConsCode tc_code)
{
   assert(tc_code != INVALID_TYPE_UID && tc_code < PStackGetSP(bank->back_idx));
   assert(PStackGetSP(bank->back_idx) == bank->names_count);

   return ((back_idx_info*)PStackElementP(bank->back_idx, tc_code))->name;
}

Type_p TypeBankParseType(Scanner_p in, TypeBank_p bank)
{
   Type_p leftArg    = NULL;
   Type_p rightArg   = NULL;
   Type_p res        = NULL;


   if (ProblemIsHO == PROBLEM_NOT_HO)
   {
      // We support only (A1 * A2 * ... * An) > B
      // or C type (constructors)

      if (!TestInpTok(in, OpenBracket))
      {
         leftArg = parse_single_type(in, bank);
         if (TestInpTok(in, GreaterSign))
         {
            AcceptInpTok(in, GreaterSign);

            rightArg = parse_single_type(in, bank);

            Type_p* args = TypeArgArrayAlloc(2);
            args[0] = leftArg;
            args[1] = rightArg;

            res = AllocArrowType(2, args);

            if (TestInpTok(in, GreaterSign))
            {
               AktTokenError(in,"Mixing of first order and higher "\
                                "order syntax is forbidden.", true);
            }
         }
         else 
         {
            res = leftArg;
         }
      }
      else
      {
         AcceptInpTok(in, OpenBracket);

         int allocated = REALLOC_STEP;
         Type_p* args = TypeArgArrayAlloc(allocated);
         args[0] = parse_single_type(in, bank);

         int arity = 1;

         do
         {
            if (TestInpTok(in, GreaterSign))
            {
               AktTokenError(in,"Mixing of first order and higher "
                                "order syntax is forbidden.", true );
            }
            AcceptInpTok(in, Mult);

            rightArg = parse_single_type(in, bank);
            if (arity == allocated)
            {
               type_arg_realloc(&args, allocated, allocated + REALLOC_STEP);
               allocated += REALLOC_STEP;
            }

            args[arity++] = rightArg;
         } while (!TestInpTok(in, CloseBracket));

         AcceptInpTok(in, CloseBracket);
         AcceptInpTok(in, GreaterSign);

         rightArg = parse_single_type(in, bank);
         if (arity == allocated)
         {
            type_arg_realloc(&args, allocated, allocated + REALLOC_STEP);
            allocated += REALLOC_STEP;
         }

         args[arity++] = rightArg;

         type_arg_realloc(&args, allocated, arity); // cut array to right size

         res = AllocArrowType(arity, args);
      }      
   }
   else 
   {
      // parsing HO types
      assert(ProblemIsHO == PROBLEM_IS_HO);
      if (TestInpTok(in, OpenBracket))
      {
         AcceptInpTok(in, OpenBracket);
         leftArg = TypeBankParseType(in, bank);
         AcceptInpTok(in, CloseBracket); 
      }
      else
      {
         leftArg = parse_single_type(in, bank);
      }

      if (TestInpTok(in, CloseBracket | Fullstop | CloseSquare | Comma 
                         | EqualSign | NegEqualSign | FOFBinOp))
      {
         res = leftArg;
      }
      else
      {
         int allocated = REALLOC_STEP;
         Type_p* args = TypeArgArrayAlloc(allocated);
         args[0] = leftArg;

         int arity = 1;

         do
         {
            if (TestInpTok(in, Mult))
            {
               AktTokenError(in,"Mixing of first order and higher "
                                "order syntax is forbidden", true);
            }
            AcceptInpTok(in, GreaterSign);
            if (TestInpTok(in, OpenBracket))
            {
               /* We can have nested arrow type */
               AcceptInpTok(in, OpenBracket);
               rightArg = TypeBankParseType(in, bank);
               AcceptInpTok(in, CloseBracket);
            }
            else
            {
               rightArg = TypeBankParseType(in, bank);
            }


            if (arity == allocated)
            {
               type_arg_realloc(&args, allocated, allocated + REALLOC_STEP);
               allocated += REALLOC_STEP;
            }

            args[arity++] = rightArg;
         } while (!(TestInpTok(in, CloseBracket | Fullstop | CloseSquare | Comma)));

         if (TypeIsArrow(rightArg))
         {
            // we have to flatten out the rightArg
            args[arity-1] = rightArg->args[0];

            int i;
            for(i=0; i < rightArg->arity-1; i++)
            {
               if (arity == allocated)
               {
                  type_arg_realloc(&args, allocated, allocated + REALLOC_STEP);
                  allocated += REALLOC_STEP;
               }
               args[arity++] = rightArg->args[i+1];
            }
         }

         type_arg_realloc(&args, allocated, arity); // cut array to right size

         res = AllocArrowType(arity, args);
      }
   }

   return TypeBankInsertTypeShared(bank, res);
}


static void __inline__ ensure_not_kind(Type_p arg, Scanner_p in)
{
   if (TypeIsKind(arg))
   {
      AktTokenError(in, "Only ground types supported.", true);
   }
}


Type_p parse_single_type(Scanner_p in, TypeBank_p bank)
{
   DStr_p         id;
   FuncSymbType   id_type;
   Type_p         type;
   Type_p*        typeArgs = NULL;
   int            arity = 0;

   id = DStrAlloc();

   id_type = TermParseOperator(in, id);
   if (id_type != FSIdentFreeFun && id_type != FSIdentInterpreted)
   {
      AktTokenError(in, "Function identifier expected", true);
   }

   if (TestInpTok(in, OpenBracket))
   {
      AcceptInpTok(in, OpenBracket);

      int allocated = REALLOC_STEP;
      typeArgs = TypeArgArrayAlloc(allocated);

      Type_p arg = parse_single_type(in, bank);
      ensure_not_kind(arg, in);

      typeArgs[arity++] = arg;

      while(TestInpTok(in, Comma))
      {
         AcceptInpTok(in, Comma);

         if (arity == allocated)
         {
            type_arg_realloc(&typeArgs, allocated, allocated + REALLOC_STEP);
            allocated += REALLOC_STEP;
         }

         arg = parse_single_type(in, bank);
         ensure_not_kind(arg, in);

         typeArgs[arity++] = arg;
      }
      AcceptInpTok(in, CloseBracket);

      //If f_code did not exist previously, it is a type constructor
      TypeConsCode tc_code;
      if ((tc_code = TypeBankFindTCCode(bank, DStrView(id))) == NAME_NOT_FOUND)
      {
         tc_code = TypeBankDefineTypeConstructor(bank, DStrView(id), arity);
      }
      else if (TypeBankFindTCArity(bank, tc_code) != arity)
      {
         DStr_p msg = DStrAlloc();
         DStrAppendStr(msg, "Redefition of type constructor ");
         DStrAppendStr(msg, (char*)TypeBankFindTCName(bank, tc_code));
         DStrAppendStr(msg, ". Mismatch in number of arguments.");
         AktTokenError(in, DStrView(msg), true);
      }

      type_arg_realloc(&typeArgs, allocated, arity); // cut array to right size
      type = TypeAlloc(tc_code, arity, typeArgs);
   }
   else
   {
      TypeConsCode tc_code;
      if ((tc_code = TypeBankFindTCCode(bank, DStrView(id))) == NAME_NOT_FOUND)
      {
         tc_code = TypeBankDefineSimpleSort(bank, DStrView(id));
      }
      else if (TypeBankFindTCArity(bank, tc_code) != 0)
      {
         DStr_p msg = DStrAlloc();
         DStrAppendStr(msg, "Type constructor ");
         DStrAppendStr(msg, (char*)TypeBankFindTCName(bank, tc_code));
         DStrAppendStr(msg, "has not been declared as simple sort.");
         AktTokenError(in, DStrView(msg), true);
      }
      type = AllocSimpleSort(tc_code);
   }


   DStrFree(id);
   return TypeBankInsertTypeShared(bank, type);
}

void TypePrintTSTP(FILE* out, TypeBank_p bank, Type_p type)
{
   if (TypeIsArrow(type))
   {
      if (ProblemIsHO == PROBLEM_NOT_HO)
      {
         int nr_of_args = type->arity -1;

         if (nr_of_args == 1)
         {
            TypePrintTSTP(out, bank, type->args[0]);
            fprintf(out, " > ");
            TypePrintTSTP(out, bank, type->args[1]);
         }
         else
         {
            fprintf(out, "(");
            for(int i=0; i<nr_of_args-1; i++)
            {
               TypePrintTSTP(out, bank, type->args[i]);
               fprintf(out, " * ");
            }
            TypePrintTSTP(out, bank, type->args[nr_of_args-1]);
            fprintf(out, ") > ");

            TypePrintTSTP(out, bank, type->args[type->arity-1]);
         }
      }
      else
      {
         assert(ProblemIsHO == PROBLEM_IS_HO);
         for(int i=0; i<type->arity-1; i++)
         {
            TypePrintTSTP(out, bank, type->args[i]);
            fprintf(out, " > ");
         }
         TypePrintTSTP(out, bank, type->args[type->arity-1]);
      }
   }
   else
   {
      fprintf(out, "%s", TypeBankFindTCName(bank, type->f_code));
      if (type->arity)
      {
         fprintf(out, "(");
         for(int i=0; i<type->arity-1; i++)
         {
            TypePrintTSTP(out, bank, type->args[i]);
            fprintf(out, ", ");
         }
         TypePrintTSTP(out, bank, type->args[type->arity-1]);
         fprintf(out, ")");
      }
   }
}

Type_p TypeChangeReturnType(TypeBank_p bank, Type_p type, Type_p new_ret)
{
   assert(TypeIsArrow(type));
   
   Type_p copy = TypeCopy(type);
   copy->args[copy->arity-1] = new_ret;

   return TypeBankInsertTypeShared(bank, copy);
}

void tree_free_fun(void* a)
{
   // Still have to figure out how types will be deleted.
   // Might have to implement GC-like mechanism.
}

void TypeBankFree(TypeBank_p bank)
{
   for(int i=0; i<PStackGetSP(bank->back_idx); i++)
   {
      back_idx_info* bii = PStackElementP(bank->back_idx, i);

      void* ptr_to_free = (void*)bii->name;

      FREE(ptr_to_free); // stiffing warnings
      SizeFree(bii, sizeof(back_idx_info));
   }

   PStackFree(bank->back_idx);

   StrTreeFree(bank->name_idx);
   for(int i=0; i<TYPEBANK_SIZE; i++)
   {
      PObjTreeFree(bank->hash_table[i], tree_free_fun);
      bank->hash_table[i] = NULL;
   }

   SizeFree(bank, sizeof(*bank));
}
