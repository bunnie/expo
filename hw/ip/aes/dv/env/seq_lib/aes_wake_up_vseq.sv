// Copyright lowRISC contributors (OpenTitan project).
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// basic wake up sequence in place to verify that environment is hooked up correctly.
// static test that is running same data set every time
class aes_wake_up_vseq extends aes_base_vseq;
  `uvm_object_utils(aes_wake_up_vseq)

  extern function new (string name = "");
  extern task body();
endclass

function aes_wake_up_vseq::new (string name = "");
  super.new(name);
endfunction

task aes_wake_up_vseq::body();
  bit [3:0] [31:0] plain_text   = 128'hDEADBEEFEEDDBBAABAADBEEFDEAFBEAD;
  bit [255:0]      init_key [2] = '{{128'h00001111222233334444555566667777,
                                     128'h88889999AAAABBBBCCCCDDDDEEEEFFFF},
                                    256'h0};
  bit              do_b2b = 0;
  bit [3:0] [31:0] decrypted_text;
  bit [3:0] [31:0] cypher_text;

  `uvm_info(`gfn, $sformatf("STARTING AES SEQUENCE"), UVM_LOW)

  `uvm_info(`gfn, $sformatf(" \n\t ---|setting operation to encrypt"), UVM_HIGH)
  // set operation to encrypt
  set_operation(AES_ENC);

  `uvm_info(`gfn,
            $sformatf({"\n\t ---| WRITING INIT KEY \n",
                       "\t ----| SHARE0 %02h \n\t ---| SHARE1 %02h "},
                      init_key[0], init_key[1]),
            UVM_HIGH)
  write_key(init_key, do_b2b);
  cfg.clk_rst_vif.wait_clks(20);

  `uvm_info(`gfn, $sformatf(" \n\t ---| ADDING PLAIN TEXT"), UVM_HIGH)

  add_data(plain_text, do_b2b);

  cfg.clk_rst_vif.wait_clks(20);
  // poll status register

  `uvm_info(`gfn, $sformatf("\n\t ---| Polling for data register %s",
                            ral.status.convert2string()), UVM_DEBUG)

  csr_spinwait(.ptr(ral.status.output_valid) , .exp_data(1'b1));
  read_data(cypher_text, do_b2b);
  // read output
  `uvm_info(`gfn, $sformatf("\n\t ------|WAIT 0 |-------"), UVM_HIGH)
  cfg.clk_rst_vif.wait_clks(20);

  // set aes to decrypt
  set_operation(AES_DEC);
  cfg.clk_rst_vif.wait_clks(20);
  `uvm_info(`gfn,
            $sformatf({"\n\t ---| WRITING INIT KEY \n",
                       "\t ----| SHARE0 %02h \n\t ---| SHARE1 %02h "},
                      init_key[0], init_key[1]),
            UVM_HIGH)
  write_key(init_key, do_b2b);
  cfg.clk_rst_vif.wait_clks(20);
  `uvm_info(`gfn, $sformatf("\n\t ---| WRITING CYPHER TEXT"), UVM_HIGH)
  add_data(cypher_text, do_b2b);


  `uvm_info(`gfn, $sformatf("\n\t ---| Polling for data %s", ral.status.convert2string()),
            UVM_DEBUG)

  cfg.clk_rst_vif.wait_clks(20);
  csr_spinwait(.ptr(ral.status.output_valid) , .exp_data(1'b1));
  read_data(decrypted_text, do_b2b);
  //need scoreboard disable
  foreach(plain_text[i]) begin
    if (plain_text[i] != decrypted_text[i]) begin
      `uvm_fatal(`gfn, $sformatf({"TEST FAILED AT POS %d:\n",
                                  "  DECRYPTED: %02h\n",
                                  "  Plaintext: %02h"},
                                 i, decrypted_text[i], plain_text[i]))
    end
  end

  foreach(decrypted_text[i]) begin
    `uvm_info(`gfn,
              $sformatf("\n\t ----| decrypted text elememt [%d] : %02h",
                        i, decrypted_text[i]), UVM_HIGH)
  end
endtask
