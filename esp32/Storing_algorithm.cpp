#include <iostream>
#include <stdint.h>
#include <vector>
#include <cassert>

#define MAX_NEIGHBORS 20
#define NEIGHBOR_TIMEOUT 2000 // ms

struct Neighbor
{
  uint8_t vehicle_id;

  float pos_x;
  float pos_y;
  float pos_z;

  float speed;
  float heading;

  uint32_t last_seen_time;
};

std::vector<Neighbor> neighbor_table;

void neighbor_table_init()
{
  neighbor_table.reserve(MAX_NEIGHBORS);
}

void update_neighbor(const Neighbor &msg)
{
  // search existing neighbor
  for (auto &n : neighbor_table)
  {
    if (n.vehicle_id == msg.vehicle_id)
    {
      n = msg;
      return;
    }
  }

  // add new neighbor
  if (neighbor_table.size() < MAX_NEIGHBORS)
  {
    neighbor_table.push_back(msg);
    return;
  }

  // table full → replace oldest
  size_t oldest_index = 0;
  uint32_t oldest_time = neighbor_table[0].last_seen_time;

  for (size_t i = 1; i < neighbor_table.size(); i++)
  {
    if (neighbor_table[i].last_seen_time < oldest_time)
    {
      oldest_time = neighbor_table[i].last_seen_time;
      oldest_index = i;
    }
  }

  neighbor_table[oldest_index] = msg;
}

void remove_stale_neighbors(uint32_t current_time)
{
  if (neighbor_table.empty())
    return;

  for (int i = neighbor_table.size() - 1; i >= 0; i--)
  {
    if (current_time - neighbor_table[i].last_seen_time > NEIGHBOR_TIMEOUT)
    {
      neighbor_table.erase(neighbor_table.begin() + i);
    }
  }
}

int main(void)
{
  neighbor_table_init();

  // Simulate receiving messages.
  // msg3 shares vehicle_id 1 with msg1 but carries NEW data, so it exercises the
  // duplicate-ID UPDATE path (the old test overwrote msg1/msg3 with ids 5/6, so that
  // path was never actually tested).
  Neighbor msg1 = {1, 10.0f, 20.0f, 0.0f, 60.0f,  90.0f, 1000};
  Neighbor msg2 = {2, 15.0f, 25.0f, 0.0f, 50.0f, 180.0f, 1500};
  Neighbor msg3 = {1, 12.0f, 22.0f, 0.0f, 65.0f,  95.0f, 2000};  // same id, new data

  update_neighbor(msg1);
  update_neighbor(msg2);
  update_neighbor(msg3);   // must UPDATE entry id=1 in place, not insert a new row

  // Two distinct vehicle ids (1 and 2) → exactly two rows.
  assert(neighbor_table.size() == 2);
  // The id-1 row must hold msg3's data (the update), not msg1's stale values.
  for (const auto &n : neighbor_table)
    if (n.vehicle_id == 1)
      assert(n.last_seen_time == 2000 && n.speed == 65.0f);

  // Simulate time passing and removing stale neighbors.
  // 4000 - 2000 == 2000 (not > 2000) keeps id 1; 4000 - 1500 == 2500 (> 2000) drops id 2.
  remove_stale_neighbors(4000);
  assert(neighbor_table.size() == 1);
  assert(neighbor_table[0].vehicle_id == 1);
  std::cout << "[TEST] all assertions passed\n";

  // Print remaining neighbors
  for (const auto &n : neighbor_table)
  {
    std::cout << "Vehicle ID: " << (int)n.vehicle_id
              << ", Position: (" << n.pos_x << ", " << n.pos_y << ", " << n.pos_z << ")"
              << ", Speed: " << n.speed
              << ", Heading: " << n.heading
              << ", Last Seen: " << n.last_seen_time << " ms" << std::endl;
  }

  return 0;
}