#include "fss.h"

// ---------------------------- HELPER FUNCTIONS ---------------------------- //
inline void xor(uint8_t *a, uint8_t *b, uint8_t *res, size_t s_len){
    for (size_t i = 0; i < s_len; i++)
    {
        res[i] = a[i] ^ b[i];
    }
}
inline void bit_decomposition(DTYPE_t value, bool *bits_array){
    for (size_t i = 0; i < N_BITS; i++)
    {
        bits_array[i] = value & (1<<(N_BITS-i-1));
    }
}
inline void xor_cond(uint8_t *a, uint8_t *b, uint8_t *res, size_t len, bool cond){
    if (cond)
    {
        xor(a, b, res, len);
    }   
    else
    {
        memcpy(res, a, len);
    }
}

// -------------------------------------------------------------------------- //
// --------------------------- RANDOMNESS SAMPLING -------------------------- //
// -------------------------------------------------------------------------- //
void init_libsodium(){
    if (sodium_init() < 0) /* panic! the library couldn't be initialized, it is not safe to use */
        {
            printf("<Funshade Error>: libsodium init failed\n");
            exit(EXIT_FAILURE);
        }
}

void init_states_random_seeded(uint8_t s0[S_LEN], uint8_t s1[S_LEN], const uint8_t seed[randombytes_SEEDBYTES]){
    if (s0==NULL || s1==NULL)
    {
        printf("<Funshade Error>: s0|s1 must have allocated memory to initialize\n");
        exit(EXIT_FAILURE);
    }
    #ifdef USE_LIBSODIUM // Secure random number generation using libsodium
        init_libsodium();
        if (seed == NULL)   // If seed is not provided, use secure randomness
        {
            randombytes_buf(s0, S_LEN);
            randombytes_buf(s1, S_LEN);
        }
        else                // Use provided seed
        {
            randombytes_buf_deterministic(s0, S_LEN, seed);
            randombytes_buf_deterministic(s1, S_LEN, seed);
        }
    #else               // Use insecure random number generation
        srand((unsigned int)time(NULL));        // Initialize random seed
        for (size_t i = 0; i < S_LEN; i++){
            s0[i] = rand() % 256;   s1[i] = rand() % 256;}
    #endif
}
void init_states_random(uint8_t s0[S_LEN], uint8_t s1[S_LEN]){
    init_states_random_seeded(s0, s1, NULL);
}

DTYPE_t init_dtype_random_seeded(const uint8_t seed[randombytes_SEEDBYTES]){
    DTYPE_t value = 0;
    #ifdef USE_LIBSODIUM // Secure random number generation using libsodium
        init_libsodium();
        if (seed == NULL)   // If seed is not provided, use secure randomness
        {
            randombytes_buf(&value, sizeof(DTYPE_t));
        }
        else                // Use provided seed
        {
            randombytes_buf_deterministic(&value, sizeof(DTYPE_t), seed);
        }
    #else               // Use insecure random number generation
        if (seed == NULL)   // If seed is not provided, use secure randomness
        {
            srand(*((unsigned int*)seed));        // Initialize seeded
        }
        else                // Use provided seed
        {
            srand((unsigned int)time(NULL));        // Initialize random seed
        }
        for (size_t i = 0; i < sizeof(DTYPE_t); i++){
            *(uint8_t*)(value+i) = rand() % 256;}
    #endif
    return value;
}
DTYPE_t init_dtype_random(){
    return init_dtype_random_seeded(NULL);
}

// -------------------------------------------------------------------------- //
// ----------------- DISTRIBUTED COMPARISON FUNCTION (DCF) ------------------ //
// -------------------------------------------------------------------------- //
void DCF_gen_seeded(DTYPE_t alpha, struct dcf_key *k0, struct dcf_key *k1, uint8_t s0[S_LEN], uint8_t s1[S_LEN]){
    // Inputs and outputs to G
    uint8_t s0_i[S_LEN] = {0},  g_out_0[G_OUT_LEN]= {0},
            s1_i[S_LEN] = {0},  g_out_1[G_OUT_LEN]= {0};
    // Pointers to the various parts of the output of G
    uint8_t *s0_keep, *s0_lose, *v0_keep, *v0_lose, *t0_keep,
            *s1_keep, *s1_lose, *v1_keep, *v1_lose, *t1_keep;
    // Temporary variables
    uint8_t CW_chain[CW_CHAIN_LEN] = {0};
    uint8_t s_cw[S_LEN] = {0};
    DTYPE_t V_cw, V_alpha=0;    bool t0=0, t1=1;                                // L3
    bool t_cw_L, t_cw_R, t0_L, t0_R, t1_L, t1_R;

    // Decompose alpha into an array of bits                                    // L1
    bool alpha_bits[N_BITS] = {0};
    bit_decomposition(alpha, alpha_bits);

    // Initialize s0 and s1 randomly if they are NULL                           // L2
    if (s0==NULL || s1==NULL)
    {
        init_states_random(s0_i, s1_i);
    }
    else
    {
        memcpy(s0_i, s0, S_LEN);
        memcpy(s1_i, s1, S_LEN);
    }
    memcpy(k0->s, s0_i, S_LEN);
    memcpy(k1->s, s1_i, S_LEN);

    // Main loop
    for (size_t i = 0; i < N_BITS; i++)                                         // L4
    {
        G_ni(s0_i, g_out_0, G_IN_LEN, G_OUT_LEN);                               // L5
        G_ni(s1_i, g_out_1, G_IN_LEN, G_OUT_LEN);                               // L6
        t0_L = TO_BOOL(g_out_0 + T_L_PTR);   t0_R = TO_BOOL(g_out_0 + T_R_PTR);
        t1_L = TO_BOOL(g_out_1 + T_L_PTR);   t1_R = TO_BOOL(g_out_1 + T_R_PTR);
        if (alpha_bits[i])  // keep = R; lose = L;                              // L8
        {
            s0_keep = g_out_0 + S_R_PTR;    s0_lose = g_out_0 + S_L_PTR;
            v0_keep = g_out_0 + V_R_PTR;    v0_lose = g_out_0 + V_L_PTR;
            t0_keep = g_out_0 + T_R_PTR;  //t0_lose = g_out_0 + T_L_PTR;
            s1_keep = g_out_1 + S_R_PTR;    s1_lose = g_out_1 + S_L_PTR;
            v1_keep = g_out_1 + V_R_PTR;    v1_lose = g_out_1 + V_L_PTR;
            t1_keep = g_out_1 + T_R_PTR;  //t1_lose = g_out_1 + T_L_PTR;
            
        }
        else                // keep = L; lose = R;                              // L7
        {
            s0_keep = g_out_0 + S_L_PTR;    s0_lose = g_out_0 + S_R_PTR;
            v0_keep = g_out_0 + V_L_PTR;    v0_lose = g_out_0 + V_R_PTR;
            t0_keep = g_out_0 + T_L_PTR;  //t0_lose = g_out_0 + T_R_PTR;
            s1_keep = g_out_1 + S_L_PTR;    s1_lose = g_out_1 + S_R_PTR;
            v1_keep = g_out_1 + V_L_PTR;    v1_lose = g_out_1 + V_R_PTR;
            t1_keep = g_out_1 + T_L_PTR;  //t1_lose = g_out_1 + T_R_PTR;
        }

        xor(s0_lose, s1_lose, s_cw, S_LEN);                                     // L10
        V_cw = (t1?-1:1) * (TO_DTYPE(v1_lose) - TO_DTYPE(v0_lose) - V_alpha);   // L11
        V_cw += alpha_bits[i] * (t1?-1:1) * BETA; // Lose=L --> alpha_bits[i]=1 // L12

        V_alpha += TO_DTYPE(v0_keep) - TO_DTYPE(v1_keep) + (t1?-1:1)*V_cw;      // L14
        t_cw_L = t0_L ^ t1_L ^ alpha_bits[i] ^ 1;                               // L15
        t_cw_R = t0_R ^ t1_R ^ alpha_bits[i];
        memcpy(CW_chain + S_CW_PTR(i), s_cw, S_LEN);                            // L16
        memcpy(CW_chain + V_CW_PTR(i), &V_cw, V_LEN);
        memcpy(CW_chain + T_CW_L_PTR(i), &t_cw_L, sizeof(bool));
        memcpy(CW_chain + T_CW_R_PTR(i), &t_cw_R, sizeof(bool));
        
        xor_cond(s0_keep, s_cw, s0_i, S_LEN, t0);                               // L18
        t0 = TO_BOOL(t0_keep) ^ (t0 & (alpha_bits[i]?t_cw_R:t_cw_L));           // L19
        xor_cond(s1_keep, s_cw, s1_i, S_LEN, t1);                                    
        t1 = TO_BOOL(t1_keep) ^ (t1 & (alpha_bits[i]?t_cw_R:t_cw_L));              
        
    }
    V_alpha = (t1?-1:1) * (TO_DTYPE(s1_i) - TO_DTYPE(s0_i) - V_alpha);          // L20
    memcpy(CW_chain+LAST_CW_PTR, &V_alpha,  sizeof(DTYPE_t));
    // Prepare the resulting keys                                               // L21
    memcpy(k0->CW_chain, CW_chain, CW_CHAIN_LEN);
    memcpy(k1->CW_chain, CW_chain, CW_CHAIN_LEN);
}
void DCF_gen(DTYPE_t alpha, struct dcf_key *k0, struct dcf_key *k1){
    DCF_gen_seeded(alpha, k0, k1, NULL, NULL);
}



DTYPE_t DCF_eval(bool b, struct dcf_key *kb, DTYPE_t x){
    DTYPE_t V = 0;                                                              // L1
    bool t = b, x_bits[N_BITS];
    uint8_t s[S_LEN], g_out[G_OUT_LEN], CW_chain[CW_CHAIN_LEN];     
    
    // Copy the key elements to avoid modifying the original key
    memcpy(s, kb->s, S_LEN);
    memcpy(CW_chain, kb->CW_chain, CW_CHAIN_LEN);
    
    // Decompose x into an array of bits
    bit_decomposition(x, x_bits);

    // Main loop
    for (size_t i = 0; i < N_BITS; i++)                                         // L2
    {
        G_ni(s, g_out, G_IN_LEN, G_OUT_LEN);                                    // L4
        if (x_bits[i]==0)  // Pick the Left branch
        {
           V += (b?-1:1) * (  TO_DTYPE(&g_out[V_L_PTR]) +                       // L7
                            t*TO_DTYPE(&CW_chain[V_CW_PTR(i)]));
           xor_cond(g_out+S_L_PTR, CW_chain+S_CW_PTR(i), s, S_LEN, t);          // L8
           t = TO_BOOL(g_out+T_L_PTR) ^ (t & TO_BOOL(CW_chain+T_CW_L_PTR(i)));
        }
        else               // Pick the Right branch
        {
           V += (b?-1:1) * (  TO_DTYPE(&g_out[V_R_PTR]) +                       // L9
                            t*TO_DTYPE(&CW_chain[V_CW_PTR(i)]));
           xor_cond(g_out + S_R_PTR, CW_chain+S_CW_PTR(i), s, S_LEN, t);        // L10  
           t = TO_BOOL(g_out+T_R_PTR) ^ (t & TO_BOOL(CW_chain+T_CW_R_PTR(i)));                    
        }
    }
    V += (b?-1:1) * (TO_DTYPE(s) + t*TO_DTYPE(CW_chain + LAST_CW_PTR));         // L13
    return V;
}

// -------------------------------------------------------------------------- //
// ------------------------- INTERVAL CONTAINMENT --------------------------- //
// -------------------------------------------------------------------------- //
void IC_gen(DTYPE_t r_in, DTYPE_t r_out, DTYPE_t p, DTYPE_t q, struct ic_key *k0_ic, struct ic_key  *k1_ic){
    DTYPE_t gamma = r_in - 1;
    struct dcf_key k0_dcf={0}, k1_dcf={0};  DCF_gen(gamma, &k0_dcf, &k1_dcf);
    DTYPE_t q_prime = q + 1, alpha_p = p + r_in, alpha_q = q + r_in, alpha_q_prime = q_prime + r_in;
    DTYPE_t z0=0, z1=0;
    z0 = init_dtype_random();
    z1 = r_out + (alpha_p>alpha_q?1:0) - (alpha_p>p?1:0) + (alpha_q_prime>q_prime?1:0) + (alpha_q_prime==0?1:0);
    k0_ic->z = z0;
    k1_ic->z = z1;
    memcpy(&k0_ic->dcf_k, &k0_dcf, sizeof(struct dcf_key));
    memcpy(&k1_ic->dcf_k, &k1_dcf, sizeof(struct dcf_key));
}

DTYPE_t IC_eval(bool b, DTYPE_t p, DTYPE_t q, struct ic_key *kb_ic, DTYPE_t x){
    DTYPE_t q_prime = q + 1, x_p = x - p - 1, x_q_prime = x + q_prime - 1;
    DTYPE_t output_1 = DCF_eval(b, &kb_ic->dcf_k, x_p);
    DTYPE_t output_2 = DCF_eval(b, &kb_ic->dcf_k, x_q_prime);
    DTYPE_t output = b * ((x>p?1:0)-(x>q_prime?1:0)) - output_1 + output_2 + kb_ic->z;
    return output;
}