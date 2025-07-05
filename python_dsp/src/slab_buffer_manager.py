import numpy as np

class SlabBuffer:
    def __init__(self, num_blocks, block_size):
        self.num_blocks = num_blocks
        self.block_size = block_size
        self.slab = np.zeros((num_blocks, block_size), dtype=np.float32)
        self.write_index = 0
        self.filled = 0
        self.epoch = 0
        self.absolute_sample_index = 0

    def get_write_block(self):
        return self.slab[self.write_index]

    def advance_write_index(self):
        self.write_index = (self.write_index + 1) % self.num_blocks
        if self.write_index == 0:
            self.epoch += 1
        self.filled = min(self.filled + 1, self.num_blocks)
        self.absolute_sample_index += self.block_size

    def get_latest_absolute_sample_index(self):
        return self.absolute_sample_index
    
    def get_capacity_samples(self):
        return self.num_blocks * self.block_size
    
    def write_block(self, block):
        self.slab[self.write_index, :] = block
        #self.advance_write_index()

    def get_relative(self, offset, sample_index):
        if not (-self.filled <= offset < 0 or offset == 0):
            raise IndexError("Offset out of valid slab history")
        idx = (self.write_index + offset - 1 + self.num_blocks) % self.num_blocks
        return self.slab[idx, sample_index]

    def get_latest_block(self):
        if self.filled == 0:
            return None
        idx = (self.write_index - 1 + self.num_blocks) % self.num_blocks
        return self.slab[idx]

    def get_latest_index(self):
        return (self.write_index - 1 + self.num_blocks) % self.num_blocks
    
    def get_epoch(self):
        return self.epoch
    
    
    def get_window(self, start_abs_idx, end_abs_idx):
        #print("attempting to get window")
        window_len = end_abs_idx - start_abs_idx
        if window_len <= 0:
            raise ValueError("Invalid window length")
        capacity = self.get_capacity_samples()
        window = np.empty(window_len, dtype=self.slab.dtype)
        for i in range(window_len):
            abs_idx = start_abs_idx + i
            rel_idx = abs_idx % capacity
            block_idx = rel_idx // self.block_size
            sample_idx = rel_idx % self.block_size
            window[i] = self.slab[block_idx, sample_idx]
        return window
    
    def extract_window(self, start_idx, end_idx, pre, post, return_none_if_too_old=True):
        #print("attempting to extract window")
        start = start_idx - pre
        end = end_idx + post
        latest = self.get_latest_absolute_sample_index()
        capacity = self.get_capacity_samples()
        # Check that both start and end are within buffer
        if latest - start > capacity or latest - end > capacity:
            print(f"exceeds capacity latest - start {latest-start}, latest - end {latest - end} ")
            if return_none_if_too_old:
                return None
            else:
                raise ValueError("Requested window is too old/not in buffer")
        return self.get_window(start, end)

