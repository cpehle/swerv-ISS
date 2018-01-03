// -*- c++ -*-

#pragma once

#include <cstdint>
#include <vector>
#include <type_traits>
#include "IntRegs.hpp"
#include "CsRegs.hpp"
#include "Memory.hpp"

namespace WdRiscv
{

  /// Model a RISCV core with registers of type URV (uint32_t for
  /// 32-bit registers and uint64_t for 64-bit registers).
  template <typename URV>
  class Core
  {
  public:
    
    enum InterruptCause
      {
	U_SOFTWARE = 0,  // User mode software interrupt
	S_SOFTWARE = 1,  // Supervisor mode software interrupt
	M_SOFTWARE = 3,  // Machine mode software interrupt
	U_TIMER    = 4,  // User mode timer interrupt
	S_TIMER    = 5,  // Supervisor
	M_TIMER    = 7,  // Machine
	U_EXTERNAL = 8,  // User mode external interrupt
	S_EXTERNAL = 9,  // Supervisor
	M_EXTERNAL = 11  // Machine
      };

    enum ExceptionCause 
      {
	INST_ADDR_MISALIGNED  = 0,
	INST_ACCESS_FAULT     = 1,
	ILLEGAL_INST          = 2,
	BREAKPOINT            = 3,
	LOAD_ADDR_MISALIGNED  = 4,
	LOAD_ACCESS_FAULT     = 5,
	STORE_ADDR_MISALIGNED = 6,
	STORE_ACCESS_FAULT    = 7,
	U_ENV_CALL            = 8,  // Environment call from user mode
	S_ENV_CALL            = 9,  // Supervisor
	M_ENV_CALL            = 11, // Machine
	INST_PAGE_FAULT       = 12,
	LOAD_PAGE_FAULT       = 13,
	STORE_PAGE_FAULT      = 15
      };

    /// Signed register type corresponding to URV. For exmaple, if URV
    /// is uint32_t, then SRV will be int32_t.
    typedef typename std::make_signed<URV>::type SRV;

    /// Constructor: Define a core with given memory size and register
    /// count.
    Core(size_t memorySize, size_t intRegCount);

    /// Destructor.
    ~Core();

    /// Return count of integer registers.
    size_t intRegCount() const
    { return intRegs_.size(); }

    /// Return size of memory in bytes.
    size_t memorySize() const
    { return memory_.size(); }

    /// Set val to the value of integer register x returning true on
    /// success. Return false leaving val unmodified if x is out of
    /// bounds.
    bool peekIntReg(unsigned ix, URV& val) const
    { 
      if (ix < intRegs_.size())
	{
	  val = intRegs_.read(ix);
	  return true;
	}
      return false;
    }

    void initialize();

    void run();

    /// Run until the program counter reaches the given address. Do not
    /// execute the instruction at that address.
    void runUntilAddress(URV address);

    /// Disassemble given instruction putting results into the given
    /// string.
    void disassembleInst(uint32_t inst, std::string& str);

    /// Helper to disassembleInst. Disassemble a 32-bit instruction.
    void disassembleInst32(uint32_t inst, std::string& str);

    /// Helper to disassembleInst. Disassemble a compressed (16-bit)
    /// instruction.
    void disassembleInst16(uint16_t inst, std::string& str);

    /// Expand given 16-bit co to the equivalent 32-bit instruction
    /// code returning true on sucess and false if given 16-bit code
    /// does not correspond to a valid instruction.
    bool expandInst(uint16_t code16, uint32_t& code32) const;

    /// Load the given hex file and set memory locations accordingly.
    /// Return true on success. Return false if file does not exists,
    /// cannot be opened or contains malformed data.
    /// File format: A line either contains @address where address
    /// is a hexadecimal memory address or one or more space separated
    /// tokens each consisting of two hexadecimal digits.
    bool loadHexFile(const std::string& file);

    /// Load the given ELF file and set memory locations accordingly.
    /// Return true on success. Return false if file does not exists,
    /// cannot be opened or contains malformed data. If successful,
    /// set entryPoint to the entry point of the loaded file.
    bool loadElfFile(const std::string& file, size_t& entryPoint);

    /// Run self test. Return true on success and false on failure.
    /// Processor state is not preserved. Neither is memory state.
    bool selfTest();

  protected:

    /// Start a synchronous exceptions.
    void initiateException(ExceptionCause cause, URV pc, URV info);

    /// Start an asynchronous exception (interrupt).
    void initiateInterrupt(InterruptCause cause, URV pc);

    /// Execute given 32-bit instruction. Assumes currPc_ is set to
    /// the address of the instruction in simulated memory. Assumes
    /// pc_ is set to currPc_ plus 4. Neither pc_ or currPc_ is used
    /// to reference simulated memory. A branch instruction an
    /// exception will end up modifying pc_.
    void execute32(uint32_t inst);

    /// Execute given 16-bit instruction. Assumes currPc_ is set to
    /// the address of the instruction in simulated memory. Assumes
    /// pc_ is set to currPc_ plus 2. Neither pc_ or currPc_ is used
    /// to reference simulated memory. A branch instruction an
    /// exception will end up modifying pc_.
    void execute16(uint16_t inst);

    /// Change machine state and program counter in reaction to an
    /// exception or an interrupt. Given pc is the program counter to
    /// save (address of instruction causing the asynchronous
    /// exception or the instruction to resume after asynchronous
    /// exception is handeled). The info value holds additional
    /// information about an exception.
    void initiateTrap(bool interrupt, URV cause, URV pcToSave, URV info);

    /// Illegal instruction:
    ///   - Machine mode instruction executed when not in machine mode.
    void illegalInst();

    // rs1: index of source register (value range: 0 to 31)
    // rs2: index of source register (value range: 0 to 31)
    // rd: index of destination register (value range: 0 to 31)
    // offset: singed integer.
    // imm: signed integer.
    // uimm: unsigned integer.
    // All immediate and offset values are assumed to be already unpacked
    // and sign extended if necessary.

    // The program counter is adjusted (size of current instruction added)
    // before any of the following functions are called. To get the address
    // before adjustment, use currPc_.
    void execBeq(uint32_t rs1, uint32_t rs2, SRV offset);
    void execBne(uint32_t rs1, uint32_t rs2, SRV offset);
    void execBlt(uint32_t rs1, uint32_t rs2, SRV offset);
    void execBltu(uint32_t rs1, uint32_t rs2, SRV offset);
    void execBge(uint32_t rs1, uint32_t rs2, SRV offset);
    void execBgeu(uint32_t rs1, uint32_t rs2, SRV offset);

    void execJalr(uint32_t rd, uint32_t rs1, SRV offset);
    void execJal(uint32_t rd, SRV offset);

    void execLui(uint32_t rd, SRV imm);
    void execAuipc(uint32_t rd, SRV imm);

    void execAddi(uint32_t rd, uint32_t rs1, SRV imm);
    void execSlli(uint32_t rd, uint32_t rs1, uint32_t amount);
    void execSlti(uint32_t rd, uint32_t rs1, SRV imm);
    void execSltiu(uint32_t rd, uint32_t rs1, URV uimm);
    void execXori(uint32_t rd, uint32_t rs1, URV uimm);
    void execSrli(uint32_t rd, uint32_t rs1, uint32_t amount);
    void execSrai(uint32_t rd, uint32_t rs1, uint32_t amount);
    void execOri(uint32_t rd, uint32_t rs1, URV uimm);
    void execAndi(uint32_t rd, uint32_t rs1, URV uimm);
    void execAdd(uint32_t rd, uint32_t rs1, uint32_t rs2);
    void execSub(uint32_t rd, uint32_t rs1, uint32_t rs2);
    void execSll(uint32_t rd, uint32_t rs1, uint32_t rs2);
    void execSlt(uint32_t rd, uint32_t rs1, uint32_t rs2);
    void execSltu(uint32_t rd, uint32_t rs1, uint32_t rs2);
    void execXor(uint32_t rd, uint32_t rs1, uint32_t rs2);
    void execSrl(uint32_t rd, uint32_t rs1, uint32_t rs2);
    void execSra(uint32_t rd, uint32_t rs1, uint32_t rs2);
    void execOr(uint32_t rd, uint32_t rs1, uint32_t rs2);
    void execAnd(uint32_t rd, uint32_t rs1, uint32_t rs2);

    void execEcall();
    void execEbreak();

    void execCsrrw(uint32_t rd, uint32_t csr, uint32_t rs1);
    void execCsrrs(uint32_t rd, uint32_t csr, uint32_t rs1);
    void execCsrrc(uint32_t rd, uint32_t csr, uint32_t rs1);
    void execCsrrwi(uint32_t rd, uint32_t csr, URV imm);
    void execCsrrsi(uint32_t rd, uint32_t csr, URV imm);
    void execCsrrci(uint32_t rd, uint32_t csr, URV imm);

    void execLb(uint32_t rd, uint32_t rs1, SRV imm);
    void execLh(uint32_t rd, uint32_t rs1, SRV imm);
    void execLw(uint32_t rd, uint32_t rs1, SRV imm);
    void execLbu(uint32_t rd, uint32_t rs1, SRV imm);
    void execLhu(uint32_t rd, uint32_t rs1, SRV imm);
    void execLwu(uint32_t rd, uint32_t rs1, SRV imm);

    void execSb(uint32_t rs1, uint32_t rs2 /*byte*/, SRV imm);
    void execSh(uint32_t rs1, uint32_t rs2 /*half*/, SRV imm);
    void execSw(uint32_t rs1, uint32_t rs2 /*word*/, SRV imm);

    void execMul(uint32_t rd, uint32_t rs1, uint32_t rs2);
    void execMulh(uint32_t rd, uint32_t rs1, uint32_t rs2);
    void execMulhsu(uint32_t rd, uint32_t rs1, uint32_t rs2);
    void execMulhu(uint32_t rd, uint32_t rs1, uint32_t rs2);
    void execDiv(uint32_t rd, uint32_t rs1, uint32_t rs2);
    void execDivu(uint32_t rd, uint32_t rs1, uint32_t rs2);
    void execRem(uint32_t rd, uint32_t rs1, uint32_t rs2);
    void execRemu(uint32_t rd, uint32_t rs1, uint32_t rs2);

  private:

    Memory memory_;
    IntRegs<URV> intRegs_;  // Integer register file.
    CsRegs<URV> csRegs_;    // Control and status registers.
    URV pc_;        // Program counter. Incremented by instruction fetch.
    URV currPc_;    // Pc of instruction being executed (pc_ before fetch).

    PrivilegeMode privilegeMode_;     // Privilige mode.
    unsigned mxlen_;
  };
}

